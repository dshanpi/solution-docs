import React, {useEffect, useId, useState} from 'react';
import Link from '@docusaurus/Link';
import styles from './styles.module.css';

const sources = [
  {name:'模块驱动',path:'bsp/drivers/vin/modules/sensor/lt6911c_mipi.c',description:'输入时序、采集格式与模块子设备。',href:'/docs/course/drivers/lt6911c-driver'},
  {name:'采集与编码',path:'kvm_video_demo/main.cpp',description:'V4L2 缓冲区、硬件编码与本地通信。',href:'/docs/course/capture/capture-loop'},
  {name:'进程与协议',path:'kvm/cmd/kvm/ · kvm/internal/app/',description:'网络服务入口、会话管理和本地协议。',href:'/docs/course/integration/service-architecture'},
  {name:'浏览器会话',path:'kvm/internal/app/webrtc.go · video_stream.go',description:'WebRTC 视频及 DataChannel 控制通道。',href:'/docs/course/integration/video-socket'},
  {name:'平台与启动',path:'kvm-platform-service/ · buildroot/100ask/userdata/100ask_kvm/',description:'系统接口、平台配置与独立进程监督。',href:'/docs/course/deployment/startup'},
  {name:'USB 键鼠',path:'kvm-hid-service/c/',description:'USB HID 服务：Gadget、报告、键盘灯与输入释放。',href:'/docs/course/usb-hid/gadget-config'},
];
const views = [{id:'all',label:'完整框架'},{id:'video',label:'画面传输'},{id:'control',label:'键鼠控制'}];
const explanations = {
  all:'画面从被控设备流向浏览器；键鼠操作沿另一条路径返回。两条链路共用 Go 服务和网络连接；USB 数据线连接 T527 的 OTG 口（Device 模式）与被控设备的 USB Host 口。',
  video:'HDMI → MIPI CSI-2 → V4L2 采集 → H.264 硬件编码 → Go 服务 → WebRTC → 浏览器。',
  control:'浏览器键鼠事件 → DataChannel → 网络服务 → 本地接口 → USB HID 服务 → /dev/hidg* → Linux HID Gadget → T527 OTG 口（Device 模式）→ USB 数据线 → 被控设备 USB Host 口。',
};
function Arrow(){return <span aria-hidden="true">↗</span>;}
const videoPaths = ['M158 217h27v-57h29','M360 160h76','M562 160h34','M724 160h32','M868 160h49v57h25'];
const controlPaths = ['M942 283h-25v-23H828v-58','M812 202v140','M650 384h-28','M486 384h-22','M368 384H184V283h-26'];
function Flow({paths,type,active}) {
  return <g className={`${styles.flow} ${type==='video' ? styles.videoFlow : styles.controlFlow}`} aria-hidden="true" data-active={active}>
    {paths.map((path,index)=><path key={path} d={path} pathLength="100" style={{animationDelay:`${index*8/paths.length-8}s`}}/>)}
  </g>;
}
function Node({x,y,w=128,h=84,title,subtitle,href,kind}) {
  return <Link to={href} aria-label={`${title}，阅读相关章节`} className={styles.diagramNode}>
    <rect x={x} y={y} width={w} height={h} rx="4"/>
    <text x={x+w/2} y={y+30} className={styles.nodeTitle}>{title}</text>
    <text x={x+w/2} y={y+52} className={styles.nodeSub}>{subtitle}</text>
    {kind && <text x={x+w/2} y={y+72} className={styles.nodeKind}>{kind}</text>}
  </Link>;
}
function SystemDiagram({view,playing}) {
  const id=useId().replace(/:/g,'');
  return <svg viewBox="0 0 1100 600" className={styles.diagram} data-playing={playing} role="img" aria-labelledby={`arch-title-${id} arch-desc-${id}`}>
    <title id={`arch-title-${id}`}>Web KVM 系统框架：{views.find(item=>item.id===view).label}</title>
    <desc id={`arch-desc-${id}`}>{explanations[view]}采集模块和 T527 主板组成 KVM 设备。图中节点可跳转相关章节。</desc>
    <defs><marker id={`video-${id}`} viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" orient="auto"><path d="m0 0 10 5-10 5z" fill="#5a7734"/></marker><marker id={`control-${id}`} viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" orient="auto"><path d="m0 0 10 5-10 5z" fill="#4e667c"/></marker></defs>
    <rect x="192" y="32" width="714" height="540" rx="6" className={styles.deviceBoundary}/>
    <text x="210" y="56" className={styles.zoneLabel}>KVM 设备</text>
    <rect x="416" y="76" width="472" height="468" rx="4" className={styles.boardBoundary}/>
    <text x="435" y="100" className={styles.zoneLabel}>T527 主板 / Linux</text>
    <text x="89" y="161" className={styles.outerLabel}>画面来源 · 操作目标</text>
    <text x="1011" y="161" className={styles.outerLabel}>远程访问入口</text>
    <Node x={20} y={184} w={138} h={128} title="被控设备" subtitle="HDMI OUT" kind="USB HOST" href="/docs/course/preparation/hardware"/>
    <Node x={942} y={184} w={138} h={128} title="操作端浏览器" subtitle="显示视频" kind="产生键鼠事件" href="/docs/course/integration/service-architecture"/>
    <g className={view==='control' ? styles.muted : ''}>
      <Node x={214} y={118} w={146} title="采集模块" subtitle="LT6911 系列" kind="HDMI → MIPI" href="/docs/course/drivers/lt6911c-driver"/>
      <Node x={436} y={118} w={126} title="VIN / V4L2" subtitle="采集接口" kind="Linux 驱动" href="/docs/course/capture/video-format"/>
      <Node x={596} y={118} title="kvm_video" subtitle="采集与编码" kind="硬件编码器" href="/docs/course/encoding/encoder-memory"/>
      <g className={styles.videoWire} markerEnd={`url(#video-${id})`}>{videoPaths.map(path=><path key={path} d={path}/>)}</g>
      <Flow paths={videoPaths} type="video" active={view!=='control'}/>
      <g className={styles.wireLabel}><text x="185" y="237">HDMI</text><text x="397" y="144">MIPI</text><text x="579" y="144">NV12</text><text x="740" y="144">H.264</text><text x="915" y="237">WebRTC</text></g>
      <text x="287" y="226" className={styles.annotation}>模块固件完成信号转换</text>
      <text x="650" y="225" className={styles.annotation}>视频在主板上采集、编码后送入 Go 服务</text>
    </g>
    <Node x={756} y={118} w={112} title="kvm_app" subtitle="会话与网络" kind="WebRTC / 本地接口" href="/docs/course/integration/service-architecture"/>
    <g className={view==='video' ? styles.muted : ''}>
      <Node x={368} y={352} w={96} h={64} title="T527 OTG" subtitle="Device 模式" href="/docs/course/preparation/wiring-firmware"/>
      <Node x={486} y={342} w={136} title="USB HID Gadget" subtitle="USB 设备功能" kind="Linux 内核" href="/docs/course/usb-hid/gadget-config"/>
      <Node x={650} y={342} w={218} title="kvm_hid" subtitle="USB HID 服务" kind="Gadget · 报告 · 状态" href="/docs/course/usb-hid/gadget-config"/>
      <g className={styles.controlWire} markerEnd={`url(#control-${id})`}>{controlPaths.map(path=><path key={path} d={path}/>)}</g>
      <Flow paths={controlPaths} type="control" active={view!=='video'}/>
      <g className={styles.controlLabel}><text x="861" y="284">DataChannel</text><text x="862" y="303">键鼠事件</text><text x="765" y="289">HID IPC</text><text x="636" y="367">hidg*</text><text x="270" y="406">USB 数据线</text><text x="270" y="423">USB HID 报告</text></g>
    </g>
    <g className={styles.platformBranch}><path d="M868 180h30v314h-30" fill="none" stroke="#8a927f" strokeWidth="1.3"/><text x="868" y="452" className={styles.annotation}>平台接口</text><Node x={650} y={454} w={218} title="kvm_platform" subtitle="终端与系统管理" kind="设备接口 · 平台配置" href="/docs/course/deployment/startup"/></g>
    <text x="535" y="593" className={styles.annotation}>I²C 用于模块控制与状态访问，图像通过 MIPI 传输。</text>
  </svg>;
}

export default function KvmArchitecture() {
  const [view,setView]=useState('all');
  const [playing,setPlaying]=useState(false);
  useEffect(()=>{
    const preference=window.matchMedia('(prefers-reduced-motion: reduce)');
    setPlaying(!preference.matches);
    const update=()=>setPlaying(!preference.matches);
    preference.addEventListener('change',update);
    return ()=>preference.removeEventListener('change',update);
  },[]);
  return <article className={styles.root}>
    <div className={styles.breadcrumb}><Link to="/docs/course/">Web KVM</Link><span>/</span><span>系统框架</span></div>
    <header className={styles.header}><div><span className={styles.eyebrow}>SYSTEM ARCHITECTURE</span><h1>KVM 系统框架</h1></div><p>看清一帧画面如何到达浏览器，<br/>一次键鼠操作如何返回设备。</p></header>
    <nav className={styles.sectionNav} aria-label="本页内容"><a href="#signal-flow">信号流向</a><a href="#responsibilities">软硬件分工</a><a href="#process-channels">进程通信</a><a href="#usb-hid-的实现分工">USB HID</a><a href="#源码入口">源码入口</a></nav>
    <section id="signal-flow" className={styles.system} aria-label="交互架构图">
      <div className={styles.diagramToolbar}><div className={styles.viewSwitch} role="group" aria-label="选择架构链路">{views.map(item=><button type="button" key={item.id} aria-pressed={view===item.id} onClick={()=>setView(item.id)}>{item.label}</button>)}</div><span className={styles.diagramHint}>点击模块，进入相关章节 <Arrow/></span></div>
      <p className={styles.scrollHint}>左右滑动查看完整架构 · 点击模块进入章节</p>
      <div className={styles.diagramScroll} tabIndex={0} role="region" aria-label="系统框架图，小屏可横向滚动"><SystemDiagram view={view} playing={playing}/></div>
      <div className={styles.diagramLegend}><span><i/> 视频数据</span><span><i/> 键鼠控制</span><small>流向示意 · 非实时数据</small><button type="button" className={styles.motionToggle} onClick={()=>setPlaying(value=>!value)} aria-label={playing ? '暂停流动演示' : '播放流动演示'}><span aria-hidden="true">{playing ? 'Ⅱ' : '▷'}</span>{playing ? '暂停演示' : '播放演示'}</button></div>
      <p className={styles.flowDescription} aria-live="polite">{explanations[view]}</p>
    </section>
    <section id="responsibilities" className={styles.section}>
      <div className={styles.sectionHeading}><span>01 / 软硬件分工</span><h2>同一套系统，各自负责什么？</h2></div>
      <div className={styles.roles}>
        <div><span>信号转换</span><h3>模块固件</h3><p>接收 HDMI 输入，转换成 MIPI CSI-2 图像，供主板采集。</p><Link to="/docs/course/preparation/wiring-firmware">接线与供电 <Arrow/></Link></div>
        <div><span>硬件接口</span><h3>Linux 驱动</h3><p>VIN / V4L2 提供采集接口；HID Gadget 负责 USB 枚举、端点与传输。</p><Link to="/docs/course/drivers/device-tree">设备树与驱动 <Arrow/></Link></div>
        <div><span>视频处理</span><h3>视频服务</h3><p>取得 NV12 帧，调用硬件编码器，向 Go 服务发送 H.264 数据。</p><Link to="/docs/course/capture/first-frame">采集与编码 <Arrow/></Link></div>
        <div><span>网络与交互</span><h3>Go 服务 + 前端</h3><p>管理网页、认证、会话与网络传输；将输入事件交给 USB HID 服务。</p><Link to="/docs/course/integration/service-architecture">服务架构 <Arrow/></Link></div>
        <div><span>输入设备</span><h3>USB HID 服务</h3><p>独占管理 Gadget、报告写入和键盘灯；断线时尝试释放按键与按钮。</p><Link to="/docs/course/usb-hid/gadget-config">USB 键鼠 <Arrow/></Link></div>
        <div><span>系统接口</span><h3>平台服务</h3><p>承接终端与系统管理接口。设备路径和板级参数由平台配置提供。</p><Link to="/docs/course/deployment/startup">部署与启动 <Arrow/></Link></div>
      </div>
      <p className={styles.teachingNote}>教程深入驱动、C / C++ 采集编码与 HID 实验；Go 和前端复用配套程序，重点介绍它们的职责与接口。</p>
    </section>
    <section id="process-channels" className={styles.section}>
      <div className={styles.sectionHeading}><span>02 / 本地进程通信</span><h2>四个服务，通过本地接口协作。</h2></div>
      <p className={styles.lead}>视频数据、采集控制、USB HID 和平台管理使用各自的本地接口。启动脚本监督各服务；功能拆分不等于权限沙箱，也不代表更换主板后无需适配。</p>
      <div className={styles.channelTable}>
        <div className={styles.channelRow}><div><b>视频通道</b><small>视频服务 → 网络服务</small></div><div className={styles.packet}><span>KVM2 · 20 字节头</span><span>H.264 数据</span></div><p>包头携带长度、采集时间戳和标志；网络服务据此生成 RTP 时间戳。</p><Link to="/docs/course/integration/video-socket">查看协议 <Arrow/></Link></div>
        <div className={styles.channelRow}><div><b>控制通道</b><small>网络服务 ⇄ 视频服务</small></div><div className={styles.commands}><code>start_video / stop_video</code><span>Go → C++：启停采集</span><code>video_input_state</code><span>C++ → Go：输入状态</span></div><p>逐行 JSON。request_keyframe 请求新 IDR；输入状态与有效视频帧分别验证。</p><Link to="/docs/course/integration/capture-control">查看控制 <Arrow/></Link></div>
        <div className={styles.channelRow}><div><b>键鼠通道</b><small>网络服务 ⇄ HID 服务</small></div><div className={styles.commands}><code>/run/kvm/hid.sock</code><span>HID IPC v1 · 逐行 JSON</span></div><p>输入状态、配置、键盘灯和释放请求。服务独占 USB 设备操作。</p><Link to="/docs/course/integration/capture-control">查看键鼠链路 <Arrow/></Link></div>
        <div className={styles.channelRow}><div><b>平台通道</b><small>网络服务 ⇄ 平台服务</small></div><div className={styles.commands}><code>/run/kvm/platform.sock</code><span>系统调用接口与终端桥接</span></div><p>配置选择设备路径；终端会话和系统操作由平台服务承接。</p><Link to="/docs/course/deployment/startup">查看部署 <Arrow/></Link></div>
      </div>
    </section>
    <section id="usb-hid-的实现分工" className={styles.section}>
      <div className={styles.sectionHeading}><span>03 / USB HID</span><h2>应用写报告，内核完成 USB 传输。</h2></div>
      <div className={styles.hidRoles}><div><h3>网络服务</h3><p>校验会话、解释浏览器输入事件，通过本地接口提交键盘和鼠标状态。</p></div><div><h3>USB HID 服务</h3><p>配置 configfs，按描述符组织报告，写入对应的 <code>/dev/hidg*</code>，读取键盘灯状态。</p></div><div><h3>Linux 内核</h3><p>HID Gadget 和 USB 控制器负责枚举与端点传输，经 T527 OTG 口（Device 模式）将报告送到被控设备的 USB Host 口。</p></div></div>
      <div className={styles.hidDevices}><span>当前项目的节点映射</span><span>键盘 <code>/dev/hidg0</code></span><span>绝对鼠标 <code>/dev/hidg1</code></span><span>相对鼠标 <code>/dev/hidg2</code></span></div>
      <p className={styles.teachingNote}>报告格式由 HID 描述符决定。服务根据 function 的 dev 属性发现节点；下列编号是当前板上的映射，不是跨平台固定值。</p>
    </section>
    <section id="源码入口" className={styles.section}>
      <div className={styles.sectionHeading}><span>04 / 源码入口</span><h2>从架构，找到实际代码。</h2></div><p className={styles.lead}>以下路径相对配套 SDK 根目录。右侧链接进入相关讲解章节。</p>
      <div className={styles.sources}>{sources.map(item=><div key={item.name} className={styles.sourceRow}><b>{item.name}</b><div><code>{item.path}</code><p>{item.description}</p></div><Link to={item.href} aria-label={`阅读${item.name}相关章节`}>阅读章节 <Arrow/></Link></div>)}</div>
    </section>
    <footer className={styles.next}><Link to="/docs/course/">← 返回方案介绍</Link><Link to="/docs/course/preparation/hardware"><span>下一步</span><b>认识 Web KVM 与硬件组成 <Arrow/></b></Link></footer>
  </article>;
}
