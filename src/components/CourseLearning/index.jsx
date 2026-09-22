import React, {useEffect, useState} from 'react';
import Link from '@docusaurus/Link';
import {useLocation} from '@docusaurus/router';
import {stages, lessons, shortStageNames, stageLabels} from './catalog';
import {normalizePath, resumePath} from './progress.mjs';
import {useProgress} from './ProgressProvider';
import styles from './styles.module.css';

const number = (lesson) => typeof lesson.number === 'number' ? String(lesson.number).padStart(2, '0') : lesson.number;
function useCurrent() {
  const {pathname} = useLocation();
  const path = normalizePath(pathname);
  const lesson = lessons.find(item => item.href === path);
  return {path, lesson, stage: stages.find(item => item.id === lesson?.stage)};
}
function Status({lesson, current}) {
  const {progress} = useProgress();
  const done = progress.completed.includes(lesson.href);
  const state = done ? '已完成' : current ? '正在阅读' : progress.visited.includes(lesson.href) ? '已浏览，未完成' : '未开始';
  return <span className={`${styles.status} ${done ? styles.done : ''} ${current ? styles.currentStatus : ''}`} aria-label={`${typeof lesson.number === 'number' ? `第 ${number(lesson)} 章` : `专题 ${lesson.number}`}，${state}`}>{number(lesson)}</span>;
}
function ProgressMeter() {
  const {progress} = useProgress();
  return <div className={styles.meter} role="progressbar" aria-label="已完成课程章节" aria-valuenow={progress.completed.length} aria-valuemin={0} aria-valuemax={lessons.length}><span style={{width:`${progress.completed.length / lessons.length * 100}%`}}/></div>;
}
export function ResumeCard() {
  const {progress, persistent} = useProgress();
  const next = lessons.find(item => item.href === resumePath(progress, lessons.map(item => item.href)));
  return <section className={styles.resume} aria-label="我的学习进度">
    <div className={styles.resumeProgress}><span>我的学习</span><strong>{String(progress.completed.length).padStart(2, '0')}<small> / {lessons.length}</small></strong><ProgressMeter/></div>
    <div className={styles.resumeText}><span>{progress.completed.length === lessons.length ? '全部章节已标记完成 · 可以随时回顾' : progress.last ? '接着上次的进度' : '从认识设备开始'}</span><b>{number(next)} · {next.title}</b><small>{persistent ? '阅读位置与完成标记保存在此浏览器' : '浏览器未允许保存，进度仅在本次打开期间有效'}</small></div>
    <Link to={next.href} className={styles.primary}>{progress.completed.length === lessons.length ? '回顾课程' : progress.last ? '继续学习' : '开始学习'} <span aria-hidden="true">→</span></Link>
  </section>;
}

export function CourseDirectory({onNavigate}) {
  const {path} = useCurrent();
  const {progress, persistent} = useProgress();
  return <nav className={styles.directory} aria-label="课程学习目录">
    <div className={styles.directoryHead}><span className={styles.kicker}>100ASK / 开发与应用指南</span><Link to="/docs/course/" onClick={onNavigate}><b>Web KVM</b></Link><div className={styles.progressLabel}><span>学习进度</span><strong>{progress.completed.length} / {lessons.length}</strong></div><ProgressMeter/><small>{persistent ? '进度自动保存在此浏览器' : '当前进度仅在本次打开期间有效'}</small></div>
    <div className={styles.overviewLinks}><Link to="/docs/course/" aria-current={path === '/docs/course' ? 'page' : undefined} onClick={onNavigate}>项目介绍 <span>↗</span></Link><Link to="/docs/course/architecture" aria-current={path === '/docs/course/architecture' ? 'page' : undefined} onClick={onNavigate}>系统框架 <span>↗</span></Link></div>
    {stages.map((stage, index) => <section className={styles.directoryStage} key={stage.id} aria-label={`${stageLabels[index]}：${stage.title}`}>
      <div className={styles.stageTitle}><span>{stageLabels[index]}</span><small>{stage.lessons.filter(item => progress.completed.includes(item.href)).length}/{stage.lessons.length} 已完成</small><h3>{stage.title}</h3></div>
      <ul className={styles.chapterList}>{stage.lessons.map(lesson => <li key={lesson.href}><Link className={styles.directoryLesson} to={lesson.href} aria-current={path === lesson.href ? 'page' : undefined} onClick={onNavigate}><Status lesson={lesson} current={path === lesson.href}/><span>{lesson.shortTitle}</span>{path === lesson.href && <i aria-hidden="true">←</i>}</Link></li>)}</ul>
    </section>)}
  </nav>;
}

export function CoursePath({compact = false}) {
  const {path, lesson, stage} = useCurrent();
  const {ready, visit, progress} = useProgress();
  const [expanded, setExpanded] = useState(false);
  useEffect(() => { if (ready && lesson) visit(lesson.href); }, [ready, lesson, visit]);
  useEffect(() => { setExpanded(false); }, [path]);
  return <div className={`${styles.pathShell} ${compact ? styles.compactPath : ''}`}>
    <div className={styles.pathHeading}><Link to="/docs/course/"><svg className={styles.courseIcon} viewBox="0 0 24 24" fill="none" aria-hidden="true"><rect x="5" y="5" width="14" height="14" rx="3" stroke="currentColor" strokeWidth="1.5"/><path d="M9 1v4m6-4v4M9 19v4m6-4v4M1 9h4m-4 6h4m14-6h4m-4 6h4" stroke="currentColor" strokeWidth="1.5"/><rect x="9" y="9" width="6" height="6" rx="1" fill="currentColor"/></svg>Web KVM <span>/ 学习路线</span></Link><button type="button" aria-expanded={expanded} aria-controls="course-all-lessons" onClick={() => setExpanded(value => !value)}>{expanded ? '收起目录 −' : '全部章节 ↗'}<small>{progress.completed.length}/{lessons.length} 已完成</small></button></div>
    {!compact && <nav className={styles.stageTrack} aria-label="按阶段跳转章节">{stages.map((item, index) => <Link key={item.id} to={item.lessons[0].href} className={stage?.id === item.id ? styles.activeStage : ''} aria-current={stage?.id === item.id ? 'step' : undefined}><span>{stageLabels[index]}{item.lessons.every(entry => progress.completed.includes(entry.href)) ? ' ✓' : ''}</span><b>{shortStageNames[index]}</b></Link>)}</nav>}
    {lesson && <nav className={styles.peerLessons} aria-label="本阶段章节"><span>{stage.title}</span><div>{stage.lessons.map(item => <Link key={item.href} to={item.href} aria-current={item.href === path ? 'page' : undefined}><Status lesson={item} current={item.href === path}/><span>{item.shortTitle}</span></Link>)}</div></nav>}
    <div id="course-all-lessons" hidden={!expanded} className={styles.expandedDirectory}><CourseDirectory onNavigate={() => setExpanded(false)}/></div>
    {lesson && <div className={styles.readingLabel}><span>当前阅读 · {typeof lesson.number === 'number' ? `第 ${number(lesson)} 章` : `进阶 ${lesson.number}`}</span><span>{progress.completed.includes(lesson.href) ? '✓ 已完成，可继续复习' : '阅读中 · 完成后可在文末标记'}</span></div>}
  </div>;
}

export function LessonFooter() {
  const {lesson} = useCurrent();
  const {progress, toggle, persistent} = useProgress();
  if (!lesson) return null;
  const index = lessons.indexOf(lesson);
  const previous = lessons[index - 1];
  const next = lessons[index + 1];
  const done = progress.completed.includes(lesson.href);
  return <section className={styles.lessonFooter} aria-label="章节学习操作">
    <div className={styles.finishRow}><div><span className={styles.kicker}>本章学习记录</span><h2>{done ? '已完成，随时回来复习。' : '完成本章学习了吗？'}</h2><p>{persistent ? '按本章验收目标确认后标记；可随时取消。' : '本次进度无法持久保存，关闭页面后可能丢失。'}</p></div><button type="button" className={done ? styles.completedButton : styles.primary} aria-pressed={done} onClick={() => toggle(lesson.href)}>{done ? '✓ 已完成 · 取消标记' : '标记本章完成'}</button></div>
    <nav className={styles.nextPrevious} aria-label="前后章节"><Link to={previous?.href || '/docs/course/'}><small>← {previous ? '上一章' : '返回课程'}</small><b>{previous?.title || '项目介绍与开发路线'}</b></Link>{next ? <Link to={next.href}><small>下一章 →</small><b>{next.title}</b></Link> : <Link to="/docs/course/"><small>回顾课程 →</small><b>查看全部学习进度</b></Link>}</nav>
  </section>;
}
export function LessonCompletionBadge({href}) {
  const {progress} = useProgress();
  if (!progress.completed.includes(href)) return null;
  return <small className={styles.inlineComplete}>✓ 已完成</small>;
}
