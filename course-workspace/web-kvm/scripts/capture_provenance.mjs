import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';

const courseRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const sdkRoot = process.env.SDK_ROOT || path.resolve(courseRoot, '../..');
const evidence = process.env.EVIDENCE_DIR || path.join(courseRoot, 'evidence');
fs.mkdirSync(evidence, {recursive: true});
const run = (command, args, options={}) => {
  const result = spawnSync(command, args, {cwd: sdkRoot, encoding: 'utf8', maxBuffer: 128*1024*1024, ...options});
  if (result.status !== 0) throw new Error(`${command} failed: ${result.stderr}`);
  return result.stdout;
};
const digest = (file, algorithm) => crypto.createHash(algorithm).update(fs.readFileSync(file)).digest('hex');
const walk = (dir) => fs.readdirSync(dir, {withFileTypes:true}).flatMap((entry) => {
  const full=path.join(dir,entry.name); const rel=path.relative(courseRoot,full);
  if (rel.startsWith('build/') || rel.startsWith('evidence/')) return [];
  return entry.isDirectory() ? walk(full) : [full];
});
const manifestXml = run('repo',['manifest','-r']);
const statusText = run('repo',['status']);
const commits = run('repo',['forall','-c','printf "%s\\t%s\\n" "$REPO_PATH" "$(git rev-parse HEAD)"']);
const diff = run('repo',['diff']);
fs.writeFileSync(path.join(evidence,'sdk-repo-manifest.xml'),manifestXml);
fs.writeFileSync(path.join(evidence,'sdk-repo-status.txt'),statusText);
fs.writeFileSync(path.join(evidence,'sdk-project-commits.tsv'),commits);
fs.writeFileSync(path.join(evidence,'sdk-dirty.patch'),diff);
const files = walk(courseRoot).sort().map((file) => ({path:path.relative(courseRoot,file),sha256:digest(file,'sha256')}));
const artifactDir=path.join(courseRoot,'build/arm64');
const artifacts=fs.existsSync(artifactDir) ? fs.readdirSync(artifactDir).sort().map((name)=>{const file=path.join(artifactDir,name);return {path:`build/arm64/${name}`,bytes:fs.statSync(file).size,md5:digest(file,'md5'),sha256:digest(file,'sha256')};}) : [];
const imageRoot=path.join(sdkRoot,'out/t527/avaota_a1');
const images=[];
const scanImages=(dir)=>{if(!fs.existsSync(dir))return;for(const entry of fs.readdirSync(dir,{withFileTypes:true})){const full=path.join(dir,entry.name);if(entry.isDirectory())scanImages(full);else if(entry.name.endsWith('.img'))images.push({path:path.relative(sdkRoot,full),bytes:fs.statSync(full).size,md5:digest(full,'md5'),sha256:digest(full,'sha256')});}};
scanImages(imageRoot);
const lock={schemaVersion:1,capturedAt:new Date().toISOString(),courseGitCommit:process.env.COURSE_GIT_COMMIT||'UNRECORDED',courseGitStatusSha256:process.env.COURSE_GIT_STATUS_SHA256||'UNRECORDED',courseSourceSha256:process.env.COURSE_SOURCE_SHA256||'UNRECORDED',sdk:{root:sdkRoot,profile:'t527/avaota_a1/buildroot/linux-5.15',manifestSha256:crypto.createHash('sha256').update(manifestXml).digest('hex'),statusSha256:crypto.createHash('sha256').update(statusText).digest('hex'),dirtyPatchSha256:crypto.createHash('sha256').update(diff).digest('hex'),projects:commits.trim().split('\n').length},sourceFiles:files,artifacts,images:images.sort((a,b)=>a.path.localeCompare(b.path))};
fs.writeFileSync(path.join(evidence,'provenance.lock.json'),JSON.stringify(lock,null,2));
console.log(`PROVENANCE_OK files=${files.length} artifacts=${artifacts.length} images=${images.length}`);
