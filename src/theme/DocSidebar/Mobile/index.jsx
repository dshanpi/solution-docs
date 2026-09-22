import React from 'react';
import {NavbarSecondaryMenuFiller} from '@docusaurus/theme-common';
import {useNavbarMobileSidebar} from '@docusaurus/theme-common/internal';
import OriginalMobile from '@theme-original/DocSidebar/Mobile';
import {CourseDirectory} from '@site/src/components/CourseLearning';
function MobileCourseMenu() {
  const sidebar = useNavbarMobileSidebar();
  return <CourseDirectory onNavigate={() => sidebar.toggle()}/>;
}
export default function MobileSidebar(props) {
  return props.path.startsWith('/docs/course')
    ? <NavbarSecondaryMenuFiller component={MobileCourseMenu} props={props}/>
    : <OriginalMobile {...props}/>;
}
