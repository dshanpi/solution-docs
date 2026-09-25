import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import crypto from 'node:crypto';
import {spawnSync} from 'node:child_process';
import {course, labs, advancedLabs} from './course-definition.mjs';

const root = path.resolve(import.meta.dirname, '../..');
const local = path.join(root, '.local/course-signing');
const output = path.join(root, 'dist-course');
fs.mkdirSync(local, {recursive: true, mode: 0o700});
fs.mkdirSync(output, {recursive: true});
const privatePath = process.env.LYNX_COURSE_SIGNING_KEY || path.join(local, 'ed25519-private.pem');
if (!fs.existsSync(privatePath)) {
  const {privateKey} = crypto.generateKeyPairSync('ed25519');
  fs.writeFileSync(privatePath, privateKey.export({type: 'pkcs8', format: 'pem'}), {mode: 0o600});
}
const privateKey = crypto.createPrivateKey(fs.readFileSync(privatePath));
const publicDer = crypto.createPublicKey(privateKey).export({type: 'spki', format: 'der'});
const publicKey = publicDer.subarray(publicDer.length - 32).toString('base64');
const hashTree = (inputs) => {
  const files = [];
  const visit = (target) => {
    const stat = fs.statSync(target);
    if (stat.isDirectory()) {
      for (const name of fs.readdirSync(target).sort()) visit(path.join(target, name));
    } else {
      files.push(target);
    }
  };
  for (const input of inputs) visit(path.join(root, input));
  const hash = crypto.createHash('sha256');
  for (const file of files.sort()) {
    hash.update(path.relative(root, file));
    hash.update('\0');
    hash.update(fs.readFileSync(file));
    hash.update('\0');
  }
  return hash.digest('hex');
};
const staging = fs.mkdtempSync(path.join(os.tmpdir(), 'lynx-course-'));
try {
  const provenancePath = path.join(root, 'provenance/sdk-reference/provenance.lock.json');
  const git = spawnSync('git', ['rev-parse', 'HEAD'], {cwd: root, encoding: 'utf8'});
  const gitStatus = spawnSync('git', ['status', '--porcelain=v1'], {cwd: root, encoding: 'utf8'});
  const sourceInputs = ['package.json', 'src/components/CourseLearning/catalog.js', 'tools/course', 'docs/course', 'static/examples/kvm', 'course-workspace', 'provenance/sdk-reference'];
  const manifest = {
    ...course, labs, advancedLabs,
    courseGitCommit: git.status === 0 ? git.stdout.trim() : 'WORKTREE',
    courseGitDirty: gitStatus.status !== 0 || gitStatus.stdout.trim().length > 0,
    courseGitStatusSha256: crypto.createHash('sha256').update(gitStatus.stdout || '').digest('hex'),
    courseSourceSha256: hashTree(sourceInputs),
    provenanceSha256: crypto.createHash('sha256').update(fs.readFileSync(provenancePath)).digest('hex'),
    generatedAt: new Date().toISOString(),
  };
  fs.writeFileSync(path.join(staging, 'manifest.json'), JSON.stringify(manifest, null, 2));
  fs.cpSync(path.join(root, 'docs/course'), path.join(staging, 'docs/course'), {recursive: true});
  fs.cpSync(path.join(root, 'static/examples/kvm'), path.join(staging, 'examples/kvm'), {recursive: true});
  fs.cpSync(path.join(root, 'course-workspace'), path.join(staging, 'workspace'), {recursive: true});
  fs.cpSync(path.join(root, 'provenance/sdk-reference'), path.join(staging, 'provenance'), {recursive: true});
  const payloadPath = path.join(staging, 'payload.zip');
  const zipped = spawnSync('zip', ['-qr', payloadPath, 'manifest.json', 'docs', 'examples', 'workspace', 'provenance'], {cwd: staging, encoding: 'utf8'});
  if (zipped.status !== 0) throw new Error(zipped.stderr || 'zip failed');
  const payload = fs.readFileSync(payloadPath);
  const digest = crypto.createHash('sha256').update(payload).digest();
  const envelope = {
    format: 'lynx-course-v1', algorithm: 'Ed25519-SHA256', publicKey,
    sha256: digest.toString('hex'), signature: crypto.sign(null, digest, privateKey).toString('base64'),
    payload: payload.toString('base64'),
  };
  const target = path.join(output, `${course.courseId}-${course.version}.lynx-course`);
  fs.writeFileSync(target, JSON.stringify(envelope));
  const targetBytes = fs.readFileSync(target);
  const packageSha256 = crypto.createHash('sha256').update(targetBytes).digest('hex');
  const packageMd5 = crypto.createHash('md5').update(targetBytes).digest('hex');
  fs.writeFileSync(`${target}.sha256`, `${packageSha256}  ${path.basename(target)}\n`);
  fs.writeFileSync(`${target}.md5`, `${packageMd5}  ${path.basename(target)}\n`);
  fs.writeFileSync(path.join(output, 'release.lock.json'), JSON.stringify({
    schemaVersion: 1,
    courseId: course.courseId,
    version: course.version,
    courseGitCommit: manifest.courseGitCommit,
    courseGitDirty: manifest.courseGitDirty,
    courseGitStatusSha256: manifest.courseGitStatusSha256,
    courseSourceSha256: manifest.courseSourceSha256,
    sdkProvenanceSha256: manifest.provenanceSha256,
    package: {file: path.basename(target), bytes: targetBytes.length, md5: packageMd5, sha256: packageSha256},
    signerFingerprintSha256: crypto.createHash('sha256').update(publicDer.subarray(publicDer.length - 32)).digest('hex'),
  }, null, 2));
  fs.writeFileSync(path.join(output, 'course-public-key.b64'), `${publicKey}\n`);
  console.log(target);
} finally {
  fs.rmSync(staging, {recursive: true, force: true});
}
