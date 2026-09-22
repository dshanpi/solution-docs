import React from 'react';
import OriginalPaginator from '@theme-original/DocItem/Paginator';
import {useDoc} from '@docusaurus/plugin-content-docs/client';
import {LessonFooter} from '@site/src/components/CourseLearning';
import {lessons} from '@site/src/components/CourseLearning/catalog';
import {normalizePath} from '@site/src/components/CourseLearning/progress.mjs';
export default function Paginator(props) {
  const {metadata} = useDoc();
  return lessons.some(lesson => lesson.href === normalizePath(metadata.permalink)) ? <LessonFooter/> : <OriginalPaginator {...props}/>;
}
