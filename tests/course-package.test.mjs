import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {course, labs} from '../tools/course/course-definition.mjs';

test('course exposes exactly 24 stable core labs', () => {
  assert.equal(course.schemaVersion, 2);
  assert.equal(course.version, '1.0.0-rc.1');
  assert.equal(labs.length, 24);
  assert.deepEqual(labs.map((lab) => lab.number), Array.from({length: 24}, (_, i) => i + 1));
  assert.equal(new Set(labs.map((lab) => lab.stepId)).size, 24);
  for (const lab of labs) {
    assert.ok(lab.learningObjective && lab.comprehensionCheck && lab.studentAction);
    assert.ok(lab.verificationRules.length && lab.requiredEvidence.length);
    assert.ok(fs.existsSync(path.resolve(`docs/course/${lab.path}.md`)));
  }
  assert.equal(course.boardProfile, 'avaota-a1-t527-lt6911c');
  const hilLabs = labs.filter((lab) => lab.hilVerification);
  assert.deepEqual(hilLabs.map((lab) => lab.number), [2, 3, 4, 8, 24]);
  for (const lab of hilLabs) {
    assert.match(lab.hilVerification.actionId, /^[a-z0-9-]+$/);
    assert.ok(lab.hilVerification.requiredEvidenceKinds.length > 0);
    assert.ok(lab.hilVerification.expectedObservations.length > 0);
  }
  const finalHil = labs[23].hilVerification;
  assert.equal(finalHil.minimumDurationSeconds, 7200);
  assert.equal(finalHil.image.md5, 'c2b21dad6a052e83332ab5adfc26db1e');
  assert.equal(finalHil.image.sha256, 'ecf3e222beaf24e7ddb89bc1caf1a5b340e9286ad49456c53824493aeefc111b');
});

test('built envelope signature detects payload tampering', () => {
  const dir = path.resolve('dist-course');
  const file = `${course.courseId}-${course.version}.lynx-course`;
  const exists = fs.existsSync(path.join(dir, file));
  assert.ok(exists, 'run npm run course:pack first');
  const envelope = JSON.parse(fs.readFileSync(path.join(dir, file), 'utf8'));
  const payload = Buffer.from(envelope.payload, 'base64');
  const digest = crypto.createHash('sha256').update(payload).digest();
  const prefix = Buffer.from('302a300506032b6570032100', 'hex');
  const key = crypto.createPublicKey({key: Buffer.concat([prefix, Buffer.from(envelope.publicKey, 'base64')]), type: 'spki', format: 'der'});
  assert.equal(digest.toString('hex'), envelope.sha256);
  assert.equal(crypto.verify(null, digest, key, Buffer.from(envelope.signature, 'base64')), true);
  payload[0] ^= 1;
  assert.notEqual(crypto.createHash('sha256').update(payload).digest('hex'), envelope.sha256);
  const release = JSON.parse(fs.readFileSync(path.join(dir, 'release.lock.json'), 'utf8'));
  const packageBytes = fs.readFileSync(path.join(dir, file));
  assert.equal(release.package.md5, crypto.createHash('md5').update(packageBytes).digest('hex'));
  assert.equal(release.package.sha256, crypto.createHash('sha256').update(packageBytes).digest('hex'));
  assert.match(release.courseGitCommit, /^[0-9a-f]{40}$/);
  assert.match(release.courseSourceSha256, /^[0-9a-f]{64}$/);
  assert.match(release.sdkProvenanceSha256, /^[0-9a-f]{64}$/);
});

test('course repository owns SDK source, tools and immutable provenance', () => {
  for (const required of [
    'course-workspace/web-kvm/Makefile',
    'course-workspace/web-kvm/scripts/acceptance.sh',
    'course-workspace/web-kvm/src/kvm_video.cpp',
    'provenance/sdk-reference/provenance.lock.json',
    'provenance/sdk-reference/sdk-repo-manifest.xml',
    'provenance/sdk-reference/sdk-project-commits.tsv',
    'provenance/sdk-reference/sdk-dirty.patch',
  ]) assert.ok(fs.existsSync(path.resolve(required)), required);
  const lock = JSON.parse(fs.readFileSync('provenance/sdk-reference/provenance.lock.json', 'utf8'));
  assert.equal(lock.schemaVersion, 1);
  assert.ok(lock.sourceFiles.length >= 20);
  assert.ok(lock.artifacts.every((item) => item.md5.length === 32 && item.sha256.length === 64));
  assert.ok(lock.images.every((item) => item.md5.length === 32 && item.sha256.length === 64));
});
