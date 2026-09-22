import React from 'react';
import OriginalLayout from '@theme-original/DocItem/Layout';
import {useDoc} from '@docusaurus/plugin-content-docs/client';
import {CoursePath} from '@site/src/components/CourseLearning';
export default function Layout(props) {
  const {metadata, frontMatter} = useDoc();
  if (frontMatter.slug === '/course/') return <>{props.children}</>;
  if (frontMatter.slug === '/course/architecture') return <>{props.children}</>;
  return <>{metadata.id.startsWith('course/') && <CoursePath/>}<OriginalLayout {...props}/></>;
}
