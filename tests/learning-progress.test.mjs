import test from 'node:test';
import assert from 'node:assert/strict';
import {emptyProgress, parseProgress, visitLesson, toggleCompleted, resumePath} from '../src/components/CourseLearning/progress.mjs';
const paths = ['/docs/course/one', '/docs/course/two', '/docs/course/three'];

test('损坏或旧版本记录不会注入未知章节或影响完成数', () => {
  assert.deepEqual(parseProgress('{bad', paths), emptyProgress());
  assert.deepEqual(parseProgress('null', paths), emptyProgress());
  const parsed = parseProgress(JSON.stringify({completed: [paths[0], paths[0]+'/', null, '/old-course'], visited:[paths[1]+'#section'],last:'https://unknown'}),paths);
  assert.deepEqual(parsed, {completed:[paths[0]],visited:[paths[1]],last:null});
});
test('浏览不自动完成，完成标记可撤销且刷新后保持', () => {
  const read = visitLesson(emptyProgress(), paths[1]);
  assert.equal(resumePath(read, paths), paths[1]);
  assert.deepEqual(read.completed, []);
  const done = toggleCompleted(read, paths[1]);
  const restored = parseProgress(JSON.stringify(done), paths);
  assert.deepEqual(restored, done);
  assert.equal(resumePath(restored, paths), paths[2]);
  assert.deepEqual(toggleCompleted(restored, paths[1]), read);
});
test('到达结尾后回到未完成章节，全部完成后仍有有效回顾入口', () => {
  assert.equal(resumePath({last:paths[2], completed:[paths[2]],visited:paths}, paths), paths[0]);
  assert.equal(resumePath({last:paths[2], completed:paths,visited:paths}, paths), paths[0]);
  assert.equal(resumePath(emptyProgress(),paths),paths[0]);
});
