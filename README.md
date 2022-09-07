# GSoC final report
- project: [Introduce WebSockets into rTorrent](https://summerofcode.withgoogle.com/programs/2022/projects/Qr45UY5M)
- Organization: [CCExtractor Development](https://ccextractor.org/)
- Contributor: [Dongyang Zheng](https://github.com/Young-Flash)
- mentor: [jesec](https://github.com/jesec)


# Introduction
This GSoC project will replace the antique SCGI protocol in rTorrent with Websocket, which will allows real-time events, less serialization/transfer overheads, better security, etc. There isn't a modern c++ websockets library that supports unix domain socket, we will add this important feature into uWebsockets. And we also replace the global mutex in libtorrent with shared_mutex to improve concurrency.

# What has done
- introduce websocket into rTorrent and implement "server push", that is, client can subscribe some specific topics, once the event occurs,server will push the notification to client automatically
  ![](https://young-flash.github.io/img/blog_pic/2022/ws_postman_screenshot.png)
- add unix domain socket support for uWebSockets, now websocket can listen on unix domain socket, with better security.
- replace global mutex in libtorrent with shared_mutex

# Link to work
- [qualification task before get selected](https://github.com/Young-Flash/translator)
- [unix domain socket support for uWebsocket](https://github.com/Young-Flash/uWebSockets)
- [replace mutex with shared_mutex in libtorrent](https://github.com/Young-Flash/libtorrent)
- [project code repo](https://github.com/CCExtractor/rtorrent-ws)
- [code commits](https://github.com/CCExtractor/rtorrent-ws/commits/master)
- [related blogs (records of the development process)](https://young-flash.github.io/2022/05/21/GSoC%202022%20Series%201/)

# Acknowledgements
Not only technical things I have learned but also the ability to think and tackle problems. Mentor jesec is very great, he gave me a lot of guidance and inspiration in the process of completing the project, work with jesec is wonderful, thanks jesec for help. Thanks Google and GSoC program and CCExtractor Development community provided such a good activity.