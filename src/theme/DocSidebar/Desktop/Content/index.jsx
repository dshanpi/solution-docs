import React, {useEffect, useRef} from 'react';
import OriginalContent from '@theme-original/DocSidebar/Desktop/Content';
import {CourseDirectory} from '@site/src/components/CourseLearning';
import styles from '@site/src/components/CourseLearning/styles.module.css';
export default function SidebarContent(props) {
  const container = useRef(null);
  useEffect(() => {
    const panel = container.current;
    const current = panel?.querySelector('a[aria-current="page"]');
    if (!current) return;
    const parentRect = panel.getBoundingClientRect();
    const itemRect = current.getBoundingClientRect();
    // 只滚动侧栏，保持正文位置；打开深层章节时仍能看到当前位置。
    if (itemRect.top < parentRect.top || itemRect.bottom > parentRect.bottom) {
      panel.scrollTop += itemRect.top - parentRect.top - panel.clientHeight / 2;
    }
  }, [props.path]);
  return props.path.startsWith('/docs/course') ? <div ref={container} className={`${styles.sidebarScroll} thin-scrollbar`}><CourseDirectory/></div> : <OriginalContent {...props}/>;
}
