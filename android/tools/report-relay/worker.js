// Gothic Launcher sends problem reports here and this Worker files them as GitHub issues.
// The GitHub token stays a Worker secret, because anything inside the app can be pulled out of the APK.
// An attached save goes to the "saves" release of the same private repository, linked from the issue.
const SAVE_SIZE = 64 * 1024 * 1024;

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (request.method === "PUT" && url.pathname === "/save") return save(request, env, url);
    if (request.method !== "POST") return new Response("Not found", { status: 404 });
    if (Number(request.headers.get("Content-Length")) > 200000) return new Response("Report too large", { status: 413 });
    const { success } = await env.LIMIT.limit({ key: request.headers.get("CF-Connecting-IP") ?? "" });
    if (!success) return new Response("Too many reports", { status: 429 });

    let report;
    try {
      report = await request.json();
    } catch {
      return new Response("Not a report", { status: 400 });
    }
    const title = typeof report?.title === "string" ? report.title.slice(0, 200) : "";
    const body = typeof report?.body === "string" ? report.body.slice(0, 65000) : "";
    if (!title || !body) return new Response("Not a report", { status: 400 });

    const issue = await github(env, `https://api.github.com/repos/${env.REPO}/issues`, { method: "POST", body: JSON.stringify({ title, body }) });
    if (!issue.ok) return new Response("GitHub refused the report", { status: 502 });
    const number = (await issue.json()).number;
    return Response.json({ number, upload: await uploadKey(env, number) });
  },
};

/** Takes one save for an issue this Worker just filed; the key proves the caller filed it. */
async function save(request, env, url) {
  const issue = Number(url.searchParams.get("issue"));
  const name = url.searchParams.get("name") ?? "";
  const size = Number(request.headers.get("Content-Length"));
  if (!Number.isInteger(issue) || issue <= 0 || !/^[\w.-]{1,64}$/.test(name)) return new Response("Not a save", { status: 400 });
  if (url.searchParams.get("key") !== await uploadKey(env, issue)) return new Response("Not a save", { status: 403 });
  if (!(size > 0)) return new Response("Not a save", { status: 411 });
  if (size > SAVE_SIZE) return new Response("Save too large", { status: 413 });

  const release = await github(env, `https://api.github.com/repos/${env.REPO}/releases/tags/saves`);
  if (!release.ok) return new Response("No release for saves", { status: 502 });
  const { id } = await release.json();

  // GitHub needs the length up front, so the body streams through without being held in memory.
  const { readable, writable } = new FixedLengthStream(size);
  request.body.pipeTo(writable);
  const asset = await github(env, `https://uploads.github.com/repos/${env.REPO}/releases/${id}/assets?name=issue-${issue}-${name}`, {
    method: "POST",
    headers: { "Content-Type": "application/octet-stream", "Content-Length": String(size) },
    body: readable,
  });
  if (!asset.ok) return new Response("GitHub refused the save", { status: 502 });
  const link = (await asset.json()).browser_download_url;

  const comment = `Save: [${name}](${link}), ${(size / 1e6).toFixed(1)} MB`;
  await github(env, `https://api.github.com/repos/${env.REPO}/issues/${issue}/comments`, { method: "POST", body: JSON.stringify({ body: comment }) });
  return new Response(null, { status: 204 });
}

function github(env, url, init = {}) {
  return fetch(url, {
    ...init,
    headers: {
      Authorization: `Bearer ${env.GITHUB_TOKEN}`,
      Accept: "application/vnd.github+json",
      "User-Agent": "opengothic-reports",
      "X-GitHub-Api-Version": "2022-11-28",
      ...init.headers,
    },
  });
}

// The token is the HMAC key: it already is the Worker's only secret and never leaves it.
async function uploadKey(env, issue) {
  const key = await crypto.subtle.importKey("raw", new TextEncoder().encode(env.GITHUB_TOKEN), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
  const mac = await crypto.subtle.sign("HMAC", key, new TextEncoder().encode(`save:${issue}`));
  return btoa(String.fromCharCode(...new Uint8Array(mac))).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
}
