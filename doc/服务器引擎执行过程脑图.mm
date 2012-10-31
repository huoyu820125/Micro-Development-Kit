<map version="1.0.0">
<!-- To view this file, download free mind mapping software FreeMind from http://freemind.sourceforge.net -->
<node CREATED="1351661107390" ID="ID_1785032423" MODIFIED="1351661818359" TEXT="NetServer::Start()">
<node CREATED="1351661424671" ID="ID_535899819" MODIFIED="1351661926281" POSITION="right" TEXT="NetEngine::Start()&#xa;&#x542f;&#x52a8;&#x7f51;&#x7edc;&#x5f15;&#x64ce;">
<node CREATED="1351661497046" ID="ID_667952850" MODIFIED="1351661928765" TEXT="">
<node CREATED="1351661539187" ID="ID_1865835980" MODIFIED="1351661914343" TEXT="virtual NetEventMonitor::Start() &#xa;&#x542f;&#x52a8;io&#x4e8b;&#x4ef6;&#x76d1;&#x542c;&#xa;linux&#x4e0b;&#x6307;&#x5411;EpollMonitor::Start()&#xa;windows&#x4e0b;&#x6307;&#x5411;IOCPMonitor::Start()">
<node CREATED="1351662537953" ID="ID_658201716" MODIFIED="1351662537953" TEXT="">
<node CREATED="1351662546140" ID="ID_1418985083" MODIFIED="1351662546140" TEXT="">
<node CREATED="1351662550234" ID="ID_329169253" MODIFIED="1351662550234" TEXT="">
<node CREATED="1351662615000" ID="ID_1401694789" MODIFIED="1351662710250" TEXT="IOCPMonitor::Start()">
<node CREATED="1351662635015" ID="ID_336347569" MODIFIED="1351662690437" TEXT="&#x521b;&#x5efa;iocp"/>
</node>
<node CREATED="1351662722281" ID="ID_1839447998" MODIFIED="1351662723937" TEXT="EpollMonitor::Start()">
<node CREATED="1351662926390" ID="ID_680517398" MODIFIED="1351662959765" TEXT="&#x521b;&#x5efa;epoll&#x53e5;&#x67c4;&#x7528;&#x4e8e;accept&#x76d1;&#x542c; &#xa;&#x521b;&#x5efa;epoll&#x53e5;&#x67c4;&#x7528;&#x4e8e;epollin&#x76d1;&#x542c; &#xa;&#x521b;&#x5efa;epoll&#x53e5;&#x67c4;&#x7528;&#x4e8e;epollout&#x76d1;&#x542c; "/>
<node CREATED="1351662759031" ID="ID_60826455" MODIFIED="1351662963406" TEXT="&#x521b;&#x5efa;&#x76d1;&#x542c;&#x7ebf;&#x7a0b;&#xa;">
<node CREATED="1351662875843" ID="ID_911621498" MODIFIED="1351662880312" TEXT="EpollMonitor::WaitAcceptEvent()">
<node CREATED="1351663042609" ID="ID_757423657" MODIFIED="1351663235187" TEXT="&#x63a5;&#x53d7;&#x8fde;&#x63a5;&#xa;&#x5c06;client&#x7684;socket&#x653e;&#x5165;accept queue&#xa;&#x53d1;&#x51fa;&#x4fe1;&#x53f7;&#x901a;&#x77e5;io&#x7ebf;&#x7a0b;&#x6c60;">
<node CREATED="1351663090843" ID="ID_1046606956" MODIFIED="1351663354812" TEXT="vritual NetEngine::NetMonitor()&#x7ebf;&#x7a0b;&#x6fc0;&#x6d3b;&#xa;EpollMonitor::WaitEvent()&#x8fd4;&#x56de;"/>
</node>
</node>
<node CREATED="1351662888281" ID="ID_1113257155" MODIFIED="1351662891671" TEXT="EpollMonitor::WaitInEvent()">
<node CREATED="1351663151593" ID="ID_33295896" MODIFIED="1351663230218" TEXT="&#x5c06;&#x53ef;&#x8bfb;&#x7684;socket&#x653e;&#x5165;in&#x4e8b;&#x4ef6;queue&#xa;&#x53d1;&#x51fa;&#x4fe1;&#x53f7;&#x901a;&#x77e5;io&#x7ebf;&#x7a0b;&#x6c60;">
<node CREATED="1351663271515" ID="ID_1798253796" MODIFIED="1351663359937" TEXT="vritual NetEngine::NetMonitor()&#x7ebf;&#x7a0b;&#x6fc0;&#x6d3b;&#xa;EpollMonitor::WaitEvent()&#x8fd4;&#x56de;"/>
</node>
</node>
<node CREATED="1351662898687" ID="ID_1668786015" MODIFIED="1351662900531" TEXT="EpollMonitor::WaitOutEvent()">
<node CREATED="1351663243390" ID="ID_1524724280" MODIFIED="1351663260218" TEXT="&#x5c06;&#x53ef;&#x5199;&#x7684;socket&#x653e;&#x5165;out&#x4e8b;&#x4ef6;queue&#xa;&#x53d1;&#x51fa;&#x4fe1;&#x53f7;&#x901a;&#x77e5;io&#x7ebf;&#x7a0b;&#x6c60;">
<node CREATED="1351663274656" ID="ID_1395478506" MODIFIED="1351663364218" TEXT="vritual NetEngine::NetMonitor()&#x7ebf;&#x7a0b;&#x6fc0;&#x6d3b;&#xa;EpollMonitor::WaitEvent()&#x8fd4;&#x56de;"/>
</node>
</node>
</node>
</node>
</node>
</node>
</node>
</node>
<node CREATED="1351661963515" ID="ID_1993704824" MODIFIED="1351662300937" TEXT="ThreadPool::Start()&#xa;&#x542f;&#x52a8;&#x5de5;&#x4f5c;&#x7ebf;&#x7a0b;&#x6c60;"/>
<node CREATED="1351662315890" ID="ID_695505181" MODIFIED="1351662492546" TEXT="ThreadPool::Accept();&#xa;IO&#x7ebf;&#x7a0b;&#x6c60;&#x63a5;&#x53d7;n&#x4e2a;IO&#x76d1;&#x542c;&#x4efb;&#x52a1;&#xa;n&#x4e2a;vritual NetEngine::NetMonitor()&#xa;n=&#x9884;&#x8bbe;&#x7684;io&#x7ebf;&#x7a0b;&#x6570;">
<node CREATED="1351663497156" ID="ID_569583823" MODIFIED="1351663589578" TEXT="n&#x4e2a;ThreadPool::ThreadFunc()&#x7ebf;&#x7a0b;&#x542f;&#x52a8;&#xa;&#x6302;&#x8d77;&#xff0c;&#x7b49;&#x5f85;ThreadPool::Accept&#x53d1;&#x8d77;&#x65b0;&#x4efb;&#x52a1;"/>
</node>
<node CREATED="1351662031375" ID="ID_1046351030" MODIFIED="1351662307328" TEXT="ThreadPool::Start()&#xa;&#x542f;&#x52a8;IO&#x7ebf;&#x7a0b;&#x6c60;">
<node CREATED="1351663371937" ID="ID_976336113" MODIFIED="1351663371937" TEXT="">
<node CREATED="1351663376390" ID="ID_1298868232" MODIFIED="1351663376390" TEXT="">
<node CREATED="1351663379062" ID="ID_1960526144" MODIFIED="1351663417687" TEXT="n&#x4e2a;vritual NetEngine::NetMonitor()&#x7ebf;&#x7a0b;&#x542f;&#x52a8;&#xa;&#xa;">
<node CREATED="1351664371234" ID="ID_1271433947" MODIFIED="1351664385750" TEXT="IOCPFrame::NetMonitor()">
<node CREATED="1351664414203" ID="ID_215815777" MODIFIED="1351664459968" TEXT="IOCPMonitor::WaitEvent()&#xa;&#x7ebf;&#x7a0b;&#x6302;&#x8d77;&#x7b49;&#x5f85;IO&#x4e8b;&#x4ef6;&#x53d1;&#x751f;">
<node CREATED="1351663615125" ID="ID_1335504832" MODIFIED="1351665029562" TEXT="IOCPMonitor::WaitEvent()&#x8fd4;&#x56de;io&#x5b8c;&#x6210;&#x4e8b;&#x4ef6;">
<node CREATED="1351663718000" ID="ID_926371675" MODIFIED="1351663725343" TEXT="accept&#x5b8c;&#x6210;">
<node CREATED="1351663747109" ID="ID_1847598419" MODIFIED="1351663788359" TEXT="NetEngine::OnConnect()">
<node CREATED="1351664030203" ID="ID_19418330" MODIFIED="1351664065078" TEXT="&#x4e1a;&#x52a1;&#x7ebf;&#x7a0b;&#x6c60;&#x589e;&#x52a0;&#x4e00;&#x4e2a;&#x65b0;&#x4efb;&#x52a1;NetServer::OnConnect()"/>
</node>
</node>
<node CREATED="1351663727468" ID="ID_947692561" MODIFIED="1351663735781" TEXT="recv&#x5b8c;&#x6210;">
<node CREATED="1351663794593" ID="ID_1621631586" MODIFIED="1351663799890" TEXT="NetEngine::OnData()">
<node CREATED="1351664094218" ID="ID_278421120" MODIFIED="1351664103062" TEXT="&#x4e1a;&#x52a1;&#x7ebf;&#x7a0b;&#x6c60;&#x589e;&#x52a0;&#x4e00;&#x4e2a;&#x65b0;&#x4efb;&#x52a1;NetServer::OnMsg()"/>
</node>
</node>
<node CREATED="1351663737859" ID="ID_475167103" MODIFIED="1351663743640" TEXT="send&#x5b8c;&#x6210;">
<node CREATED="1351663802015" ID="ID_1487964088" MODIFIED="1351663939078" TEXT="NetEngine::OnSend()">
<node CREATED="1351664119343" ID="ID_1880656209" MODIFIED="1351664173015" TEXT="&#x5982;&#x679c;&#x53d1;&#x9001;&#x7f13;&#x51b2;&#x6709;&#x672a;&#x53d1;&#x9001;&#x6570;&#x636e;&#xff0c;&#x518d;&#x6295;&#x9012;&#x4e00;&#x4e2a;send&#x64cd;&#x4f5c;&#xff0c;&#x7b49;&#x5f85;&#x53d1;&#x9001;&#x5b8c;&#x6210;"/>
</node>
</node>
<node CREATED="1351663957359" ID="ID_578221551" MODIFIED="1351663964015" TEXT="&#x8fde;&#x63a5;&#x5173;&#x95ed;">
<node CREATED="1351663968781" ID="ID_188259281" MODIFIED="1351663978562" TEXT="NetEngine::OnClose()">
<node CREATED="1351664182796" ID="ID_634806738" MODIFIED="1351664191171" TEXT="&#x4e1a;&#x52a1;&#x7ebf;&#x7a0b;&#x6c60;&#x589e;&#x52a0;&#x4e00;&#x4e2a;&#x65b0;&#x4efb;&#x52a1;NetServer::OnClose()"/>
</node>
</node>
</node>
</node>
</node>
<node CREATED="1351664357203" ID="ID_1594151380" MODIFIED="1351664397500" TEXT="EpollFrame::NetMonitor()">
<node CREATED="1351663432625" ID="ID_1341232733" MODIFIED="1351665004328" TEXT="&#x540d;&#x8bcd;&#x5b9a;&#x4e49;&#xff1a;&#xa;&#x9;&#x5c31;&#x7eea;socket&#xff0c;&#x53ef;&#x4ee5;recv&#x6216;&#x53ef;&#x4ee5;send&#x7684;socket&#xa;&#x9;&#x5c31;&#x7eea;&#x5217;&#x8868;&#xff0c;&#x6240;&#x6709;&#x5c31;&#x7eea;socket&#x7684;&#x5217;&#x8868;&#xa;&#xa;&#x521b;&#x5efa;&#x5c31;&#x7eea;&#x5217;&#x8868;&#xa;&#xa;1.&#x5982;&#x679c;&#x5c31;&#x7eea;socket&#x5217;&#x8868;&#x4e3a;&#x7a7a;&#xa;&#x9;EpollMonitor::WaitEvent()&#xa;&#x9;&#x7ebf;&#x7a0b;&#x6302;&#x8d77;&#x7b49;&#x5f85;&#x5c31;&#x7eea;socket&#xa;&#x5426;&#x5219;&#x9;&#xa;&#x9;&#x5f02;&#x6b65;&#x65b9;&#x5f0f;EpollMonitor::WaitEvent()&#xff0c;&#x53d6;&#x51fa;&#x65b0;&#x7684;&#x5c31;&#x7eea;socket&#xa;&#xa;&#x904d;&#x5386;&#x5c31;&#x7eea;&#x5217;&#x8868;&#x8fdb;&#x884c;io&#xff0c;&#x6bcf;&#x4e2a;io&#x6700;&#x5927;4096byte&#xff0c;&#x4fdd;&#x8bc1;&#x6240;&#x6709;socket&#x83b7;&#x5f97;&#x5e73;&#x7b49;&#x7684;io&#x673a;&#x4f1a;,&#x5bf9;&#x4e8e;recv&#x4e8b;&#x4ef6;&#xff0c;&#x63a5;&#x6536;&#x5b8c;&#x6bd5;&#x540e;&#x8c03;&#x7528;NetEngine::OnData&#xa;&#x56de;&#x5230;1&#xff0c;&#x53d6;&#x7684;&#x65b0;&#x7684;&#x5c31;&#x7eea;socket&#xff0c;&#x4fdd;&#x8bc1;&#x6240;&#x6709;socket&#x6709;&#x5e73;&#x7b49;io&#x673a;&#x4f1a;&#xa;">
<node CREATED="1351663611312" ID="ID_1252983887" MODIFIED="1351663611312" TEXT="">
<node CREATED="1351663629515" ID="ID_377530491" MODIFIED="1351663699500" TEXT="EpollMonitor::WaitEvent()&#x8fd4;&#x56de;io&#x4e8b;&#x4ef6;">
<node CREATED="1351663814062" ID="ID_1865282652" MODIFIED="1351663823031" TEXT="accept&#x5b8c;&#x6210;">
<node CREATED="1351663876687" ID="ID_1957685896" MODIFIED="1351663878593" TEXT="NetEngine::OnConnect()">
<node CREATED="1351664200218" ID="ID_1588179467" MODIFIED="1351664202750" TEXT="&#x4e1a;&#x52a1;&#x7ebf;&#x7a0b;&#x6c60;&#x589e;&#x52a0;&#x4e00;&#x4e2a;&#x65b0;&#x4efb;&#x52a1;NetServer::OnConnect()"/>
</node>
</node>
<node CREATED="1351663836359" ID="ID_1470904458" MODIFIED="1351663856671" TEXT="&#x6709;&#x6570;&#x636e;&#x7b49;&#x5f85;recv">
<node CREATED="1351663904453" ID="ID_213621270" MODIFIED="1351664913171" TEXT="&#x52a0;&#x5165;&#x5c31;&#x7eea;&#x5217;&#x8868;&#xff0c;&#x8bbe;&#x7f6e;&#x4e8b;&#x4ef6;&#x4e3a;&#x53ef;&#x8bfb;"/>
</node>
<node CREATED="1351663826156" ID="ID_254918664" MODIFIED="1351663867859" TEXT="&#x6709;&#x6570;&#x636e;&#x7b49;&#x5f85;send">
<node CREATED="1351663907734" ID="ID_550992721" MODIFIED="1351664911546" TEXT="&#x52a0;&#x5165;&#x5c31;&#x7eea;&#x5217;&#x8868;&#xff0c;&#x8bbe;&#x7f6e;&#x4e8b;&#x4ef6;&#x4e3a;&#x53ef;&#x5199;"/>
</node>
</node>
</node>
<node CREATED="1351665009531" ID="ID_1384247372" MODIFIED="1351665022781" TEXT="NetEngine:OnData()">
<node CREATED="1351665026703" ID="ID_682424812" MODIFIED="1351665036140" TEXT="&#x4e1a;&#x52a1;&#x7ebf;&#x7a0b;&#x6c60;&#x589e;&#x52a0;&#x4e00;&#x4e2a;&#x65b0;&#x4efb;&#x52a1;NetServer::OnMsg()"/>
</node>
</node>
</node>
</node>
</node>
</node>
</node>
<node CREATED="1351662072218" ID="ID_1298612168" MODIFIED="1351662091875" TEXT="&#x76d1;&#x542c;&#x6240;&#x6709;&#x9884;&#x8bbe;&#x7aef;&#x53e3;"/>
<node CREATED="1351662097578" ID="ID_1516103598" MODIFIED="1351662107718" TEXT="&#x8fde;&#x63a5;&#x6240;&#x6709;&#x9884;&#x8bbe;&#x670d;&#x52a1;&#x5668;"/>
<node CREATED="1351662142468" ID="ID_746948344" MODIFIED="1351662227125" TEXT="&#x542f;&#x52a8;&#x5f15;&#x64ce;&#x540e;&#x53f0;&#x7ebf;&#x7a0b;NetEngine::Main&#xa;&#x76d1;&#x542c;&#x5fc3;&#x8df3;&#xa;&#x81ea;&#x52a8;&#x91cd;&#x8fde;&#x7f51;&#x7edc;&#x5f02;&#x5e38;&#x7684;&#x670d;&#x52a1;&#x5668;&#xa;"/>
</node>
</node>
<node CREATED="1351661401765" ID="ID_38518225" MODIFIED="1351662138562" POSITION="right" TEXT="&#x542f;&#x52a8;&#x7ebf;&#x7a0b;vritual NetServer::main() &#xa;&#x7528;&#x6237;&#x5b9e;&#x73b0;&#xff0c;&#x670d;&#x52a1;&#x5668;&#x4e1a;&#x52a1;&#x4e3b;&#x7ebf;&#x7a0b;"/>
</node>
</map>
