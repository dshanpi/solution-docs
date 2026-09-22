import React from 'react';
import Link from '@docusaurus/Link';
import {lessons} from '../CourseLearning/catalog';
import heroArtwork from './hero-approved-transparent.png';
import LearningRoadmap from './LearningRoadmap';
import desktopImage from '@site/docs/course/images/introduction/kvm-workspace.png';
import styles from './styles.module.css';

function Arrow({diagonal = false}) {
  return <svg width="24" height="24" viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d={diagonal ? 'M5 19 19 5M5 5h14v14' : 'M4 12h15m-6-6 6 6-6 6'} stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round"/></svg>;
}

export default function KvmCourseLanding() {
  return <div className={styles.root}>
    <header className={styles.hero}>
      <div className={styles.heroInner}>
        <div className={styles.heroGrid}>
          <div className={styles.heroCopy}>
            <div className={styles.wordmark}>Web KVM<span>↗</span></div>
            <h1>设备在远端。<br/>操作在眼前。</h1>
            <p>把 HDMI 画面带到浏览器，<br/>把键盘和鼠标连接到远端设备。<br/>从动手搭建，到开发自己的 KVM 应用。</p>
            <div className={styles.actions}><a className={styles.primary} href="#solution">了解这套方案 <Arrow/></a><Link className={styles.textLink} to={lessons[0].href}>开始动手 <Arrow diagonal/></Link></div>
          </div>
          <div className={styles.artwork}><img src={heroArtwork} width="1402" height="1122" alt="Web KVM：被控设备通过 HDMI 和 USB 连接 KVM；KVM 经虚线所示网络连接浏览器，键盘鼠标位于浏览器操作端。" fetchPriority="high"/></div>
        </div>
        <div className={styles.heroBottom}><span>HDMI 画面采集</span><span>USB 键鼠控制</span><span>浏览器访问</span><a href="#course-route">探索开发指南 <span aria-hidden="true">↓</span></a></div>
      </div>
    </header>
    <section id="solution" className={styles.solution} aria-labelledby="solution-title">
      <div className={styles.sectionHeading}><div><span className={styles.kicker}>01 / 看见它，控制它</span><h2 id="solution-title">一个浏览器，<br/>就是你的操作台。</h2></div><p>设备留在工作台，画面来到你面前。<br/>通过 HDMI 获取桌面，通过 USB 发送键鼠操作，<br/>被控设备无需安装本项目的远程桌面软件。</p></div>
      <div className={styles.showcase}>
        <div className={styles.demoImage}><div className={styles.demoBar}><span><i/><i/><i/></span><b>WEB KVM / 远端桌面</b><span>实机截图</span></div><img src={desktopImage} alt="Web KVM 实机运行画面：中文工具栏与被控开发板的 Armbian 桌面" loading="lazy" width="1912" height="948"/></div>
        <div className={styles.demoCopy}><span className={styles.demoEyebrow}>从接口到体验</span><h3>看得到。<br/>也能操作。</h3><p>桌面画面、键盘输入、鼠标移动，汇集在同一个网页。硬件采集与网络服务协同，让远端操作成为一套可开发的嵌入式项目。</p><Link to="/docs/course/preparation/reference-image">查看运行与验证方法 <Arrow diagonal/></Link><div className={styles.signalStrip}><span>设备</span><b>HDMI + USB</b><span>KVM</span><b>网络</b><span>浏览器</span></div></div>
      </div>
      <div className={styles.useCases}>
        <div><span>FOR YOUR WORKBENCH</span><h3>给开发板一个远程操作台</h3><p>查看图形桌面、输入命令，减少在不同设备之间切换键鼠和显示器。</p></div>
        <div><span>FOR YOUR NEXT BUILD</span><h3>做一个看得见成果的项目</h3><p>把 Linux 驱动、视频采集与 USB 控制，组合成可以实际使用的系统。</p></div>
        <div><span>FOR YOUR OWN APPLICATION</span><h3>为自己的应用继续开发</h3><p>了解软硬件如何协作，再沿着清晰的接口适配硬件、集成服务。</p></div>
      </div>
    </section>
    <section className={styles.develop} aria-labelledby="develop-title"><div className={styles.developContent}><div className={styles.developInner}><div><span className={styles.kicker}>02 / 用起来，也弄明白</span><h2 id="develop-title">一套 KVM 方案。<br/>一条动手开发的路线。</h2></div><div><p>从硬件连接开始，深入驱动、采集、编码与 USB HID。配合现有的 Go 服务和 Web 界面，把局部实验接回完整的远程操作体验。</p><Link to="/docs/course/architecture">拆开看看系统框架 <Arrow diagonal/></Link></div></div><div className={styles.techLine}><span>HDMI → MIPI</span><span>Linux / V4L2</span><span>C / C++</span><span>H.264</span><span>USB HID</span><span>WebRTC</span></div></div></section>
    <LearningRoadmap/>
    <div className={styles.footnote}><span>WEB KVM · 开发与应用指南</span><p>当前以 T527 + LT6911 系列模块为参考实现；进阶能力和实验验证状态见对应章节。</p><a href="#solution">回到方案概览 ↑</a></div>
  </div>;
}
