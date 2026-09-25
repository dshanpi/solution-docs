import fs from 'node:fs';
import path from 'node:path';
import {labs} from './course-definition.mjs';

const root = path.resolve(import.meta.dirname, '../..');
const begin = '<!-- LYNX-LAB:BEGIN -->';
const end = '<!-- LYNX-LAB:END -->';

for (const lab of labs) {
  const file = path.join(root, 'docs/course', `${lab.path}.md`);
  const current = fs.readFileSync(file, 'utf8');
  const block = `${begin}\n\n## AI 辅助实践闭环\n\n- **稳定步骤**：\`${lab.stepId}\`\n- **学习目标**：${lab.learningObjective}\n- **理解检查**：${lab.comprehensionCheck}\n- **学生操作**：${lab.studentAction}\n- **工具范围**：${lab.toolScope.map((item) => `\`${item}\``).join('、')}\n- **风险级别**：\`${lab.riskLevel}\`\n- **预期现象**：${lab.expectedObservation}\n\n确定性验证：\n\n\`\`\`bash\ncd tutorial-examples/web-kvm\n${lab.verificationRules[0].command}\n\`\`\`\n\n必须保存命令退出码、产物摘要和证据来源；模拟结果只能标记为 \`simulation\`。完成后回答：${lab.reflectionPrompt}\n\n<details>\n<summary>分级提示</summary>\n\n1. ${lab.hintLevels.L1}\n2. ${lab.hintLevels.L2}\n3. ${lab.hintLevels.L3}\n4. ${lab.hintLevels.L4}\n\n</details>\n\n${end}`;
  const pattern = new RegExp(`${begin}[\\s\\S]*?${end}`);
  const next = pattern.test(current) ? current.replace(pattern, block) : `${current.trimEnd()}\n\n${block}\n`;
  fs.writeFileSync(file, next);
}
console.log(`Synchronized ${labs.length} structured labs.`);
