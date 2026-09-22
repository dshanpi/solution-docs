import React, {useRef, useState} from 'react';
import Link from '@docusaurus/Link';
import {stages, lessons, stageLabels} from '../CourseLearning/catalog';
import {useProgress} from '../CourseLearning/ProgressProvider';
import {resumePath} from '../CourseLearning/progress.mjs';
import styles from './roadmap.module.css';

const titles = ['连接硬件', '适配驱动', '采集图像', '编码视频', '控制键鼠', '接入网页', '部署验收', '继续探索'];
const outcomes = ['认清接口，搭好实验环境', '让 Linux 识别采集模块', '获得第一帧原始图像', '生成可播放的 H.264', '发出真实的键盘鼠标操作', '在浏览器中看画面、发操作', '从手动运行到系统交付', '热插拔、分辨率与性能'];
const tags = ['HDMI / USB', 'DEVICE TREE', 'V4L2 / NV12', 'H.264', 'USB HID', 'WEBRTC', 'LINUX SERVICE', 'MORE IDEAS'];

function Arrow() {
  return <svg viewBox="0 0 24 24" width="22" height="22" fill="none" aria-hidden="true"><path d="M4 12h15m-6-6 6 6-6 6" stroke="currentColor" strokeWidth="1.6"/></svg>;
}

// 每个阶段用真实操作对象作图形提示，章节链接和状态仍由课程目录提供。
function StageArt({index}) {
  const drawings = [
    <g key="hardware"><path d="M30 19h82v64H30z" fill="#e8efdc"/><path d="M55 34h31v32H55z" fill="#c8f343"/><path d="M19 35h17v17H19zM104 53h17v21h-17z" fill="#f8f8ef"/><path d="M65 34v-8m12 8v-8M65 66v8m12-8v8M55 44h-8m8 12h-8m39-12h8m-8 12h8"/><path d="M19 43H9v44h36M120 63h12V19h-10" fill="none"/><circle cx="41" cy="73" r="3" fill="#18220f"/><circle cx="101" cy="28" r="3" fill="#18220f"/></g>,
    <g key="driver"><path d="M34 12h52l18 18v61H34z" fill="#f8f8ef"/><path d="M86 12v19h18M47 31h22M47 42h35" fill="none"/><path d="m50 57-8 8 8 8m36-16 8 8-8 8m-16-22-7 32" fill="none"/><rect x="98" y="54" width="27" height="28" fill="#c8f343"/><path d="M107 49v5m10-5v5m-10 28v6m10-6v6m-19-24h-6m6 10h-6m33-10h6m-6 10h6"/></g>,
    <g key="capture"><path d="M18 31h84v57H18z" fill="#d6e3c3"/><path d="M28 22h84v57H28z" fill="#e8efdc"/><path d="M39 12h84v57H39z" fill="#f8f8ef"/><path d="m45 62 22-26 16 18 14-15 20 23z" fill="#c8f343"/><circle cx="104" cy="28" r="7" fill="#c8f343"/><path d="M9 17V8h15M119 81h14V67" fill="none"/></g>,
    <g key="encode"><rect x="29" y="15" width="85" height="69" rx="3" fill="#f8f8ef"/><path d="M29 30h85M29 69h85"/><path d="m61 38 22 12-22 12z" fill="#c8f343"/><path d="M39 20v5m15-5v5m15-5v5m15-5v5m15-5v5M39 74v5m15-5v5m15-5v5m15-5v5m15-5v5"/><path d="M12 43h14m-7-6 7 6-7 6m98 7h14m-7-6 7 6-7 6" fill="none"/></g>,
    <g key="hid"><rect x="10" y="38" width="89" height="45" rx="4" fill="#f8f8ef"/><path d="M20 48h5m8 0h5m8 0h5m8 0h5m8 0h5m8 0h5M20 59h5m8 0h5m8 0h5m8 0h5m8 0h5m8 0h5M31 73h49" strokeWidth="3"/><path d="M106 42q0-12 13-12t13 12v29q0 14-13 14t-13-14z" fill="#c8f343"/><path d="M119 31v17m-13 4h26M53 38V17h54" fill="none"/></g>,
    <g key="web"><rect x="25" y="12" width="96" height="65" rx="3" fill="#f8f8ef"/><path d="M25 27h96M34 20h2m5 0h2m5 0h2"/><path d="m69 35 10 28 5-10 10-5z" fill="#c8f343"/><path d="M45 40h13M45 48h8M62 78v12h25" fill="none"/><circle cx="20" cy="79" r="11" fill="#c8f343"/><path d="M16 79h8m-4-4v8M29 71l12-12" fill="none"/></g>,
    <g key="deploy"><path d="m28 36 43-20 44 20-44 20z" fill="#e8efdc"/><path d="M28 36v44l43 20 44-20V36L71 56z" fill="#f8f8ef"/><path d="M71 56v44m-20-75 43 20v20" fill="none"/><circle cx="110" cy="30" r="19" fill="#c8f343"/><path d="m100 30 7 7 13-14" fill="none" strokeWidth="3"/></g>,
    <g key="advanced"><path d="M36 28h70v57H36z" fill="#f8f8ef"/><path d="M48 41h46M48 57h46M48 72h46"/><circle cx="62" cy="41" r="6" fill="#c8f343"/><circle cx="82" cy="57" r="6" fill="#c8f343"/><circle cx="57" cy="72" r="6" fill="#c8f343"/><path d="M119 10v20m-10-10h20M15 51v12m-6-6h12M85 6v12m-6-6h12" fill="none"/></g>,
  ];
  return <svg viewBox="0 0 144 110" fill="none" stroke="#18220f" strokeWidth="2" strokeLinejoin="round" strokeLinecap="round" aria-hidden="true">{drawings[index]}</svg>;
}

export default function LearningRoadmap() {
  const {progress, persistent} = useProgress();
  const next = lessons.find(item => item.href === resumePath(progress, lessons.map(item => item.href)));
  const [chosen, setChosen] = useState(null);
  const detailRef = useRef(null);
  function selectStage(id) {
    setChosen(id);
    // 小屏路线较长，让选中后的章节入口进入视野；桌面已可见时不滚动。
    requestAnimationFrame(() => detailRef.current?.scrollIntoView({block:'nearest', behavior:'instant'}));
  }
  const selected = stages.find(stage => stage.id === (chosen || next.stage)) || stages[0];
  const selectedIndex = stages.indexOf(selected);
  const doneInStage = selected.lessons.filter(lesson => progress.completed.includes(lesson.href)).length;
  const start = selected.lessons.find(lesson => lesson.href === next.href) || selected.lessons.find(lesson => !progress.completed.includes(lesson.href)) || selected.lessons[0];
  return <section id="course-route" className={styles.section} aria-labelledby="route-title">
    <div className={styles.heading}><div><span className={styles.kicker}>03 / 现在，开始动手</span><h2 id="route-title">从第一根线，<br/>到远程操作。</h2></div><div className={styles.intro}><p>沿着路线，把一套 KVM 逐步搭起来。<br/>点击阶段，选择你要开始的章节。</p><div className={styles.overall}><span>我的学习进度</span><b>{progress.completed.length}<small> / {lessons.length}</small></b><Link to={next.href}>{progress.completed.length === lessons.length ? '回顾章节' : progress.last ? '继续学习' : '开始学习'} <Arrow/></Link></div><div className={styles.totalTrack} role="progressbar" aria-label="已完成章节" aria-valuemin={0} aria-valuemax={lessons.length} aria-valuenow={progress.completed.length}><span style={{width:`${progress.completed.length / lessons.length * 100}%`}}/></div></div></div>
    <div className={styles.mapShell}>
      <div className={styles.mapMeta}><span>LEARNING MAP / 学习路线</span><span>7 个基础阶段 <i/> 4 个进阶专题</span></div>
      <div className={styles.map}>
        <svg className={styles.connections} viewBox="0 0 1000 502" preserveAspectRatio="none" aria-hidden="true"><g fill="none" stroke="currentColor" strokeWidth="1.5"><path d="M200 76h100m-7-5 7 5-7 5M450 76h100m-7-5 7 5-7 5M700 76h100m-7-5 7 5-7 5M875 225v35m-5-7 5 7 5-7M800 344H700m7-5-7 5 7 5M550 344H450m7-5-7 5 7 5"/><path d="M300 344H200m7-5-7 5 7 5" strokeDasharray="4 5"/></g></svg>
        {stages.map((stage, index) => {
          const completed = stage.lessons.filter(lesson => progress.completed.includes(lesson.href)).length;
          const active = selected.id === stage.id;
          const done = completed === stage.lessons.length;
          const current = next.stage === stage.id && progress.completed.length < lessons.length;
          return <button key={stage.id} type="button" className={styles.stop} style={{'--column':index < 4 ? index+1 : 8-index,'--row':index < 4 ? 1 : 2,'--mobile-column':index % 4 === 0 || index % 4 === 3 ? 1 : 2,'--mobile-row':Math.floor(index / 2)+1}} data-selected={active} data-done={done} aria-pressed={active} aria-controls="roadmap-chapters" onClick={() => selectStage(stage.id)} aria-label={`${stageLabels[index]}：${titles[index]}，${completed}/${stage.lessons.length} 已完成，查看章节`}>
            <span className={styles.stopTop}><span>{stageLabels[index]}</span>{done ? <b>已完成 ✓</b> : current ? <b>{progress.last ? '接着学 ↗' : '从这里开始'}</b> : <span>{stage.lessons.length} 节</span>}</span>
            <span className={styles.scene}><StageArt index={index}/></span><strong>{titles[index]}</strong><span className={styles.outcome}>{outcomes[index]}</span>
            <span className={styles.stageProgress} aria-hidden="true">{stage.lessons.map(lesson => <i key={lesson.href} data-done={progress.completed.includes(lesson.href)}/>)}</span>
          </button>;
        })}
      </div>
      <div className={styles.mapLegend}><span>按箭头顺序学习 · 进阶专题可按需探索</span><span><i/> 已完成 <i className={styles.emptyDot}/> 待学习</span></div>
    </div>
    <div id="roadmap-chapters" ref={detailRef} className={styles.detail} role="region" aria-labelledby="selected-stage-title">
      <div className={styles.detailIntro}><span>{stageLabels[selectedIndex]} / {tags[selectedIndex]}</span><h3 id="selected-stage-title">{selected.title}</h3><p>{outcomes[selectedIndex]}。</p><Link to={start.href}>{doneInStage === selected.lessons.length ? '回顾这一阶段' : '进入这一阶段'} <Arrow/></Link><small>{doneInStage} / {selected.lessons.length} 章已完成</small></div>
      <nav className={styles.chapters} aria-label={`${selected.title}的章节`}>{selected.lessons.map(lesson => <Link to={lesson.href} key={lesson.href}><span className={styles.chapterNumber}>{typeof lesson.number === 'number' ? `第 ${String(lesson.number).padStart(2,'0')} 章` : `专题 ${lesson.number}`}</span><b>{lesson.shortTitle}</b><span className={styles.chapterState}>{progress.completed.includes(lesson.href) ? '已完成 ✓' : lesson.href === progress.last ? '上次读到' : '开始阅读'}</span><Arrow/></Link>)}</nav>
    </div>
    <div className={styles.note}><p>具备基本 C 语言与 Linux 操作经验即可开始。Go 与前端介绍系统职责和接口，复用配套程序。</p><span>{persistent ? '学习进度自动保存在此浏览器' : '当前进度仅在本次打开期间有效'}</span></div>
  </section>;
}
