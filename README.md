# 解决方案文档站点

使用 Docusaurus 构建，保留原站点的 Logo、样式、搜索功能和静态资源。Web KVM 开发与应用指南包含项目介绍、系统框架、24 章基础正文和 4 个进阶专题初稿，配有原理图、实际界面截图、流程图和可下载实验示例。部分硬件实验、驱动改造与发布镜像仍待验证。

需要 Node.js 20 或更高版本，依赖由 pnpm 管理。

```bash
pnpm install
pnpm run start
```

生产构建：

```bash
pnpm run build
```

`src/theme/` 下 swizzle 的组件会导入 `@docusaurus/plugin-content-docs/client`、`@docusaurus/theme-common` 等传递依赖，这些包未在 package.json 中直接声明。根目录 `.npmrc` 因此设置了 `shamefully-hoist=true`，让它们仍可解析；提升出来的入口都指向 `.pnpm` 中的同一份副本，不会产生重复实例。改动 `.npmrc` 后需重新执行一次 `pnpm install` 重建 node_modules。

课程正文位于 `docs/`，导航配置位于 `sidebars.js`。课程入口为 `docs/course/index.md`，各阶段使用独立目录和 `_category_.json`，侧边栏自动按目录元数据生成。原有图片及配套资源仍保留在 `docs/images/`。

## 部署配置

发布地址为 https://solution.100ask.org。GitHub Actions 会构建 `build/`，一份发布到 GitHub Pages，另一份通过 SSH 复制到服务器的 `/www/wwwroot/solution.100ask.org/`。服务器上的 Nginx 站点和 HTTPS 证书需要在 DNS 生效后单独配置；仓库不保存服务器凭据。
