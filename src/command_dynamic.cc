// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2005-2011, Jari Sundell <jaris@ifi.uio.no>

#include <vector>

#include <torrent/utils/log.h>
#include <torrent/utils/option_strings.h>

#include "command_helpers.h"
#include "control.h"
#include "globals.h"
#include "rpc/parse.h"
#include "rpc/parse_options.h"

static const std::vector<std::pair<const char*, int>> object_storage_flags = {
  { "multi", rpc::object_storage::flag_multi_type },
  { "simple", rpc::object_storage::flag_function_type },
  { "value", rpc::object_storage::flag_value_type },
  { "bool", rpc::object_storage::flag_bool_type },
  { "string", rpc::object_storage::flag_string_type },
  { "list", rpc::object_storage::flag_list_type },

  { "static", rpc::object_storage::flag_static },
  { "private", rpc::object_storage::flag_private },
  { "const", rpc::object_storage::flag_constant },
  { "rlookup", rpc::object_storage::flag_rlookup }
};

static int
object_storage_parse_flag(const std::string& flag) {
  for (auto f : object_storage_flags)
    if (f.first == flag)
      return f.second;

  throw torrent::input_error("unknown flag");
}

std::string
system_method_generate_command(torrent::Object::list_const_iterator first,
                               torrent::Object::list_const_iterator last) {
  std::string command;

  while (first != last) {
    if (!command.empty())
      command += " ;";

    command += (first++)->as_string();
  }

  return command;
}

void
system_method_generate_command2(torrent::Object*                     object,
                                torrent::Object::list_const_iterator first,
                                torrent::Object::list_const_iterator last) {
  if (first == last) {
    // TODO: Use empty object.
    *object = "";
    return;
  }

  if (first->is_string()) {
    std::string command;

    while (first != last) {
      if (!command.empty())
        command += " ;";

      command += (first++)->as_string();
    }

    *object = command;

    return;
  }

  if (first + 1 == last) {
    if (!first->is_dict_key())
      throw torrent::input_error("New command of wrong type.");

    *object = *first;

    uint32_t flags = object->flags() & torrent::Object::mask_function;
    object->unset_flags(torrent::Object::mask_function);
    object->set_flags((flags >> 1) & torrent::Object::mask_function);

  } else {
    *object = torrent::Object::create_list();

    while (first != last) {
      if (!first->is_dict_key())
        throw torrent::input_error("New command of wrong type.");

      object->as_list().push_back(*first++);

      uint32_t flags =
        object->as_list().back().flags() & torrent::Object::mask_function;
      object->as_list().back().unset_flags(torrent::Object::mask_function);
      object->as_list().back().set_flags((flags >> 1) &
                                         torrent::Object::mask_function);
    }
  }
}

// torrent::Object
// system_method_insert_function(const torrent::Object::list_type& args, int
// flags) {

// }

torrent::Object
system_method_insert_object(const torrent::Object::list_type& args, int flags) {
  if (args.empty())
    throw torrent::input_error("Invalid argument count.");

  torrent::Object::list_const_iterator itrArgs = args.begin();
  const std::string&                   rawKey  = (itrArgs++)->as_string();

  if (rawKey.empty() ||
      control->object_storage()->find_local(torrent::raw_string::from_string(
        rawKey)) != control->object_storage()->end(0) ||
      rpc::commands.has(rawKey) || rpc::commands.has(rawKey + ".set"))
    throw torrent::input_error("Invalid key.");

  torrent::Object value;

  switch (flags & rpc::object_storage::mask_type) {
    case rpc::object_storage::flag_bool_type:
    case rpc::object_storage::flag_value_type:
      value =
        itrArgs != args.end() ? rpc::convert_to_value(*itrArgs) : int64_t();
      break;
    case rpc::object_storage::flag_string_type:
      value = itrArgs != args.end() ? rpc::convert_to_string(*itrArgs) : "";
      break;
    case rpc::object_storage::flag_function_type:
      system_method_generate_command2(&value, itrArgs, args.end());
      break;
    case rpc::object_storage::flag_list_type:
    case rpc::object_storage::flag_multi_type:
      break;
    default:
      throw torrent::input_error("Invalid type.");
  }

  int cmd_flags = rpc::CommandMap::flag_delete_key;

  if (!(flags & rpc::object_storage::flag_static))
    cmd_flags |= rpc::CommandMap::flag_modifiable;
  if (!(flags & rpc::object_storage::flag_private))
    cmd_flags |= rpc::CommandMap::flag_public;

  if ((flags & rpc::object_storage::mask_type) ==
      rpc::object_storage::flag_list_type) {
    torrent::Object             valueList     = torrent::Object::create_list();
    torrent::Object::list_type& valueListType = valueList.as_list();

    if ((itrArgs)->is_list())
      valueListType = (itrArgs)->as_list();

    control->object_storage()->insert_str(rawKey, valueList, flags);
  } else {
    control->object_storage()->insert_str(rawKey, value, flags);
  }

  if ((flags & rpc::object_storage::mask_type) ==
        rpc::object_storage::flag_function_type ||
      (flags & rpc::object_storage::mask_type) ==
        rpc::object_storage::flag_multi_type) {

    rpc::commands.insert_slot<rpc::command_base_is_type<
      rpc::command_base_call<rpc::target_type>>::type>(
      create_new_key(rawKey),
      [rawKey](const auto& target, const auto& object) {
        return control->object_storage()->call_function_str(
          rawKey, target, object);
      },
      &rpc::command_base_call<rpc::target_type>,
      cmd_flags,
      nullptr,
      nullptr);

  } else {
    rpc::commands.insert_slot<rpc::command_base_is_type<
      rpc::command_base_call<rpc::target_type>>::type>(
      create_new_key(rawKey),
      [rawKey](const auto&, const auto&) {
        return control->object_storage()->get_str(rawKey);
      },
      &rpc::command_base_call<rpc::target_type>,
      cmd_flags,
      nullptr,
      nullptr);
  }

  // TODO: Next... Make test class for this.

  //   // Ehm... no proper handling if these throw.

  if (!(flags & rpc::object_storage::flag_constant)) {
    switch (flags & rpc::object_storage::mask_type) {
      case rpc::object_storage::flag_bool_type:
        rpc::commands.insert_slot<rpc::command_base_is_type<
          rpc::command_base_call_value<rpc::target_type>>::type>(
          create_new_key<5>(rawKey, ".set"),
          [rawKey](const auto&, const auto& object) {
            return control->object_storage()->set_str_bool(rawKey, object);
          },
          &rpc::command_base_call_value<rpc::target_type>,
          cmd_flags,
          nullptr,
          nullptr);
        break;
      case rpc::object_storage::flag_value_type:
        rpc::commands.insert_slot<rpc::command_base_is_type<
          rpc::command_base_call_value<rpc::target_type>>::type>(
          create_new_key<5>(rawKey, ".set"),
          [rawKey](const auto&, const auto& object) {
            return control->object_storage()->set_str_value(rawKey, object);
          },
          &rpc::command_base_call_value<rpc::target_type>,
          cmd_flags,
          nullptr,
          nullptr);
        break;
      case rpc::object_storage::flag_string_type:
        rpc::commands.insert_slot<rpc::command_base_is_type<
          rpc::command_base_call_string<rpc::target_type>>::type>(
          create_new_key<5>(rawKey, ".set"),
          [rawKey](const auto&, const auto& object) {
            return control->object_storage()->set_str_string(rawKey, object);
          },
          &rpc::command_base_call_string<rpc::target_type>,
          cmd_flags,
          nullptr,
          nullptr);
        break;
      case rpc::object_storage::flag_list_type:
        rpc::commands.insert_slot<rpc::command_base_is_type<
          rpc::command_base_call_list<rpc::target_type>>::type>(
          create_new_key<5>(rawKey, ".set"),
          [rawKey](const auto&, const auto& object) {
            return control->object_storage()->set_str_list(rawKey, object);
          },
          &rpc::command_base_call_list<rpc::target_type>,
          cmd_flags,
          nullptr,
          nullptr);
        break;
      case rpc::object_storage::flag_function_type:
      case rpc::object_storage::flag_multi_type:
      default:
        break;
    }
  }

  return torrent::Object();
}

// method.insert <generic> {name, "simple|private|const", ...}
// method.insert <generic> {name, "multi|private|const"}
// method.insert <generic> {name, "value|private|const"}
// method.insert <generic> {name, "value|private|const", value}
// method.insert <generic> {name, "bool|private|const"}
// method.insert <generic> {name, "bool|private|const", bool}
// method.insert <generic> {name, "string|private|const"}
// method.insert <generic> {name, "string|private|const", string}
//
// Add a new user-defined method called 'name' and any number of
// lines.
//
// TODO: Make a static version of this that doesn't need to be called
// as a command, and which takes advantage of static const char
// strings, etc.
torrent::Object
system_method_insert(const torrent::Object::list_type& args) {
  if (args.empty() || ++args.begin() == args.end())
    throw torrent::input_error("Invalid argument count.");

  torrent::Object::list_const_iterator itrArgs = args.begin();
  const std::string&                   rawKey  = (itrArgs++)->as_string();

  if (rawKey.empty() || rpc::commands.has(rawKey))
    throw torrent::input_error("Invalid key.");

  int new_flags =
    rpc::parse_option_flags(itrArgs->as_string(), [](const auto& flag) {
      return object_storage_parse_flag(flag);
    });

  torrent::Object::list_type new_args;
  new_args.push_back(rawKey);

  if ((new_flags & rpc::object_storage::flag_function_type) ||
      (new_flags & rpc::object_storage::flag_multi_type)) {
    new_args.push_back(system_method_generate_command(++itrArgs, args.end()));

  } else if ((new_flags & rpc::object_storage::flag_value_type) ||
             (new_flags & rpc::object_storage::flag_bool_type) ||
             (new_flags & rpc::object_storage::flag_string_type) ||
             (new_flags & rpc::object_storage::flag_list_type)) {
    if (++itrArgs != args.end())
      new_args.insert(new_args.end(), itrArgs, args.end());

  } else {
    throw torrent::input_error("No object type specified.");
  }

  return system_method_insert_object(new_args, new_flags);
}

// method.erase <> {name}
//
// Erase a modifiable method called 'name. Trying to remove methods
// that aren't modifiable, e.g. defined by rtorrent or set to
// read-only by the user, will result in an error.
torrent::Object
system_method_erase(const torrent::Object::string_type& args) {
  rpc::CommandMap::iterator itr = rpc::commands.find(args.c_str());

  if (itr == rpc::commands.end())
    return torrent::Object();

  if (!rpc::commands.is_modifiable(itr))
    throw torrent::input_error("Command not modifiable.");

  rpc::commands.erase(itr);

  return torrent::Object();
}

torrent::Object
system_method_redirect(const torrent::Object::list_type& args) {
  if (args.size() != 2)
    throw torrent::input_error("Invalid argument count.");

  std::string new_key  = torrent::object_create_string(args.front());
  std::string dest_key = torrent::object_create_string(args.back());

  rpc::commands.create_redirect(create_new_key(new_key),
                                create_new_key(dest_key),
                                rpc::CommandMap::flag_public |
                                  rpc::CommandMap::flag_delete_key |
                                  rpc::CommandMap::flag_modifiable);

  return torrent::Object();
}

torrent::Object
system_method_set_function(const torrent::Object::list_type& args) {
  if (args.empty())
    throw torrent::input_error("Invalid argument count.");

  rpc::object_storage::local_iterator itr =
    control->object_storage()->find_local(
      torrent::raw_string::from_string(args.front().as_string()));

  if (itr == control->object_storage()->end(0) ||
      itr->second.flags & rpc::object_storage::flag_constant)
    throw torrent::input_error("Command is not modifiable.");

  return control->object_storage()->set_str_function(
    args.front().as_string(),
    system_method_generate_command(++args.begin(), args.end()));
}

torrent::Object
system_method_has_key(const torrent::Object::list_type& args) {
  if (args.empty() || ++args.begin() == args.end())
    throw torrent::input_error("Invalid argument count.");

  torrent::Object::list_const_iterator itrArgs = args.begin();
  const std::string&                   key     = (itrArgs++)->as_string();
  const std::string&                   cmd_key = (itrArgs++)->as_string();

  return control->object_storage()->has_str_multi_key(key, cmd_key);
}

torrent::Object
system_method_set_key(const torrent::Object::list_type& args) {
  if (args.empty() || ++args.begin() == args.end())
    throw torrent::input_error("Invalid argument count.");

  torrent::Object::list_const_iterator itrArgs = args.begin();
  const std::string&                   key     = (itrArgs++)->as_string();
  const std::string&                   cmd_key = (itrArgs++)->as_string();

  if (itrArgs == args.end()) {
    control->object_storage()->erase_str_multi_key(key, cmd_key);
    return torrent::Object();
  }

  if (itrArgs->is_dict_key() || itrArgs->is_list())
    control->object_storage()->set_str_multi_key_obj(
      key.c_str(), cmd_key, *itrArgs);
  else
    control->object_storage()->set_str_multi_key(
      key, cmd_key, system_method_generate_command(itrArgs, args.end()));

  return torrent::Object();
}

torrent::Object
system_method_list_keys(const torrent::Object::string_type& args) {
  const torrent::Object::map_type& multi_cmd =
    control->object_storage()->get_str(args).as_map();

  torrent::Object             rawResult = torrent::Object::create_list();
  torrent::Object::list_type& result    = rawResult.as_list();

  for (torrent::Object::map_const_iterator itr  = multi_cmd.begin(),
                                           last = multi_cmd.end();
       itr != last;
       itr++)
    result.push_back(itr->first);

  return rawResult;
}

torrent::Object
cmd_catch(rpc::target_type target, const torrent::Object& args) {
  try {
    return rpc::call_object(args, target);
  } catch (torrent::input_error& e) {
    lt_log_print(torrent::LOG_WARN, "Caught exception: '%s'.", e.what());
    return torrent::Object();
  }
}

#define CMD2_METHOD_INSERT(key, flags)                                         \
  CMD2_ANY_LIST(key, [](const auto&, const auto& args) {                       \
    return system_method_insert_object(args, flags);                           \
  }, false);

void
initialize_command_dynamic() {
  CMD2_VAR_BOOL("method.use_deprecated", true, false);
  CMD2_VAR_VALUE("method.use_intermediate", 1, false);

  CMD2_ANY_LIST("method.insert", [](const auto&, const auto& args) {
    return system_method_insert(args);
  }, false);
  CMD2_ANY_LIST("method.insert.value", [](const auto&, const auto& args) {
    return system_method_insert_object(args,
                                       rpc::object_storage::flag_value_type);
  }, false);
  CMD2_ANY_LIST("method.insert.bool", [](const auto&, const auto& args) {
    return system_method_insert_object(args,
                                       rpc::object_storage::flag_bool_type);
  }, false);
  CMD2_ANY_LIST("method.insert.string", [](const auto&, const auto& args) {
    return system_method_insert_object(args,
                                       rpc::object_storage::flag_string_type);
  }, false);
  CMD2_ANY_LIST("method.insert.list", [](const auto&, const auto& args) {
    return system_method_insert_object(args,
                                       rpc::object_storage::flag_list_type);
  }, false);

  CMD2_METHOD_INSERT("method.insert.simple",
                     rpc::object_storage::flag_function_type);
  CMD2_METHOD_INSERT("method.insert.c_simple",
                     rpc::object_storage::flag_constant |
                       rpc::object_storage::flag_function_type);
  CMD2_METHOD_INSERT("method.insert.s_c_simple",
                     rpc::object_storage::flag_static |
                       rpc::object_storage::flag_constant |
                       rpc::object_storage::flag_function_type);

  CMD2_ANY_STRING("method.erase", [](const auto&, const auto& args) {
    return system_method_erase(args);
  }, false);
  CMD2_ANY_LIST("method.redirect", [](const auto&, const auto& args) {
    return system_method_redirect(args);
  }, false);
  CMD2_ANY_STRING("method.get", [](const auto&, const auto& str) {
    return control->object_storage()->get_str(str);
  }, false);
  CMD2_ANY_LIST("method.set", [](const auto&, const auto& args) {
    return system_method_set_function(args);
  }, false);

  CMD2_ANY_STRING("method.const", [](const auto&, const auto& key) {
    return control->object_storage()->has_flag_str(
      key, rpc::object_storage::flag_constant);
  }, false);
  CMD2_ANY_STRING_V("method.const.enable", [](const auto&, const auto& key) {
    return control->object_storage()->enable_flag_str(
      key, rpc::object_storage::flag_constant);
  }, false);

  CMD2_ANY_LIST("method.has_key", [](const auto&, const auto& args) {
    return system_method_has_key(args);
  }, false);
  CMD2_ANY_LIST("method.set_key", [](const auto&, const auto& args) {
    return system_method_set_key(args);
  }, false);
  CMD2_ANY_STRING("method.list_keys", [](const auto&, const auto& args) {
    return system_method_list_keys(args);
  }, false);

  CMD2_ANY_STRING("method.rlookup", [](const auto&, const auto& cmd_key) {
    return control->object_storage()->rlookup_obj_list(cmd_key);
  }, false);
  CMD2_ANY_STRING_V("method.rlookup.clear",
                    [](const auto&, const auto& cmd_key) {
                      return control->object_storage()->rlookup_clear(cmd_key);
                    }, false);

  CMD2_ANY("catch", [](const auto& target, const auto& args) {
    return cmd_catch(target, args);
  }, true);

  CMD2_ANY("strings.choke_heuristics", [](const auto&, const auto&) {
    return torrent::option_list_strings(torrent::OPTION_CHOKE_HEURISTICS);
  }, true);
  CMD2_ANY("strings.choke_heuristics.upload", [](const auto&, const auto&) {
    return torrent::option_list_strings(
      torrent::OPTION_CHOKE_HEURISTICS_UPLOAD);
  }, true);
  CMD2_ANY("strings.choke_heuristics.download", [](const auto&, const auto&) {
    return torrent::option_list_strings(
      torrent::OPTION_CHOKE_HEURISTICS_DOWNLOAD);
  }, true);
  CMD2_ANY("strings.connection_type", [](const auto&, const auto&) {
    return torrent::option_list_strings(torrent::OPTION_CONNECTION_TYPE);
  }, true);
  CMD2_ANY("strings.encryption", [](const auto&, const auto&) {
    return torrent::option_list_strings(torrent::OPTION_ENCRYPTION);
  }, true);
  CMD2_ANY("strings.ip_filter", [](const auto&, const auto&) {
    return torrent::option_list_strings(torrent::OPTION_IP_FILTER);
  }, true);
  CMD2_ANY("strings.ip_tos", [](const auto&, const auto&) {
    return torrent::option_list_strings(torrent::OPTION_IP_TOS);
  }, true);
  CMD2_ANY("strings.log_group", [](const auto&, const auto&) {
    return torrent::option_list_strings(torrent::OPTION_LOG_GROUP);
  }, true);
  CMD2_ANY("strings.tracker_event", [](const auto&, const auto&) {
    return torrent::option_list_strings(torrent::OPTION_TRACKER_EVENT);
  }, true);
  CMD2_ANY("strings.tracker_mode", [](const auto&, const auto&) {
    return torrent::option_list_strings(torrent::OPTION_TRACKER_MODE);
  }, true);
}
