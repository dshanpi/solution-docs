import React, {createContext, useCallback, useContext, useEffect, useState} from 'react';
import {lessons} from './catalog';
import {STORAGE_KEY, emptyProgress, parseProgress, visitLesson, toggleCompleted} from './progress.mjs';

const Context = createContext(null);
const paths = lessons.map(lesson => lesson.href);
export function ProgressProvider({children}) {
  const [progress, setProgress] = useState(emptyProgress);
  const [ready, setReady] = useState(false);
  const [persistent, setPersistent] = useState(true);
  useEffect(() => {
    try { setProgress(parseProgress(window.localStorage.getItem(STORAGE_KEY), paths)); }
    catch { setPersistent(false); }
    setReady(true);
    const sync = (event) => {
      if (event.key === STORAGE_KEY || event.key === null) setProgress(parseProgress(event.newValue, paths));
    };
    window.addEventListener('storage', sync);
    return () => window.removeEventListener('storage', sync);
  }, []);
  useEffect(() => {
    if (!ready) return;
    try { window.localStorage.setItem(STORAGE_KEY, JSON.stringify(progress)); }
    catch { setPersistent(false); }
  }, [progress, ready]);
  const visit = useCallback((path) => {
    if (paths.includes(path)) setProgress(value => visitLesson(value, path));
  }, []);
  const toggle = useCallback((path) => {
    if (paths.includes(path)) setProgress(value => toggleCompleted(value, path));
  }, []);
  return <Context.Provider value={{progress, ready, persistent, visit, toggle}}>{children}</Context.Provider>;
}
export const useProgress = () => useContext(Context);
