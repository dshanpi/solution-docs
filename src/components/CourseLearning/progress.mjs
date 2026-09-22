export const STORAGE_KEY = '100ask:t527-kvm:learning:v1';
export const normalizePath = (path = '') => path.split(/[?#]/)[0].replace(/\/+$/, '');
export const emptyProgress = () => ({completed: [], visited: [], last: null});

// 只接纳当前课程中的路径，旧版本或损坏的本地记录不会破坏导航。
export function parseProgress(raw, validPaths) {
  try {
    const value = JSON.parse(raw);
    if (!value || typeof value !== 'object') return emptyProgress();
    const clean = (list) => [...new Set((Array.isArray(list) ? list : [])
      .filter(item => typeof item === 'string').map(normalizePath).filter(path => validPaths.includes(path)))];
    return {completed: clean(value.completed), visited: clean(value.visited),
      last: typeof value.last === 'string' && validPaths.includes(normalizePath(value.last)) ? normalizePath(value.last) : null};
  } catch { return emptyProgress(); }
}

export function visitLesson(state, path) {
  if (state.last === path && state.visited.includes(path)) return state;
  return {...state, last: path, visited: [...new Set([...state.visited, path])]};
}

export function toggleCompleted(state, path) {
  return {...state, completed: state.completed.includes(path)
    ? state.completed.filter(item => item !== path) : [...state.completed, path]};
}

export function resumePath(state, paths) {
  if (state.last && paths.includes(state.last) && !state.completed.includes(state.last)) return state.last;
  const afterLast = paths.slice(paths.indexOf(state.last) + 1);
  return afterLast.find(path => !state.completed.includes(path))
    || paths.find(path => !state.completed.includes(path)) || paths[0];
}
