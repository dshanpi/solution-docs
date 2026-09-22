import React from 'react';
import {useLocation} from '@docusaurus/router';
import OriginalLayout from '@theme-original/DocRoot/Layout';

export default function DocRootLayout(props) {
  const {pathname} = useLocation();
  if (pathname.replace(/\/+$/, '') === '/docs/course') {
    return <div className="kvm-landing-main"><OriginalLayout {...props}/></div>;
  }
  return <OriginalLayout {...props}/>;
}
