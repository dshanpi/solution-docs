import React from 'react';
import {ProgressProvider} from '@site/src/components/CourseLearning/ProgressProvider';
export default function Root({children}) {
  return <ProgressProvider>{children}</ProgressProvider>;
}
