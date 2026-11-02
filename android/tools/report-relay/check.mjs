// node check.mjs: runs the Worker against a stand-in for GitHub and the rate limit
import assert from "node:assert";
import worker from "./worker.js";

// Node has no FixedLengthStream; a pass-through stream is enough here
globalThis.FixedLengthStream ??= class extends TransformStream { constructor() { super(); } };

let calls = [];
globalThis.fetch = async (url, init) => {
  calls.push({ url, init });
  if (url.endsWith("/releases/tags/saves")) return Response.json({ id: 5 });
  if (url.startsWith("https://uploads.github.com/")) {
    await new Response(init.body).arrayBuffer();
    return Response.json({ browser_download_url: "https://github.com/owner/reports/releases/download/saves/issue-7-save_quick_1.sav" }, { status: 201 });
  }
  return Response.json({ number: 7 }, { status: 201 });
};
const sent = () => calls[calls.length - 1];
let allowed = true;
const env = { REPO: "owner/reports", GITHUB_TOKEN: "token", LIMIT: { limit: async () => ({ success: allowed }) } };
const post = (body) => new Request("https://relay.example/", { method: "POST", body: typeof body === "string" ? body : JSON.stringify(body) });

let r = await worker.fetch(post({ title: "Gothic crashed", body: "log" }), env);
assert.equal(r.status, 200);
const filedReport = await r.json();
assert.equal(filedReport.number, 7);
assert.ok(filedReport.upload);
assert.equal(sent().url, "https://api.github.com/repos/owner/reports/issues");
assert.equal(sent().init.headers.Authorization, "Bearer token");
assert.deepEqual(JSON.parse(sent().init.body), { title: "Gothic crashed", body: "log" });

calls = [];
assert.equal((await worker.fetch(post("not json"), env)).status, 400);
assert.equal((await worker.fetch(post({ title: "", body: "log" }), env)).status, 400);
assert.equal((await worker.fetch(post({ title: 1, body: "log" }), env)).status, 400);
assert.equal((await worker.fetch(new Request("https://relay.example/"), env)).status, 404);
assert.equal(calls.length, 0, "Nothing reaches GitHub for a bad request");

r = await worker.fetch(post({ title: "x".repeat(500), body: "y".repeat(70000) }), env);
const filed = JSON.parse(sent().init.body);
assert.equal(filed.title.length, 200);
assert.equal(filed.body.length, 65000);

const put = (query, data = "save data") => new Request(`https://relay.example/save?${query}`, {
  method: "PUT", body: data, headers: { "Content-Length": String(data.length) },
});
calls = [];
assert.equal((await worker.fetch(put(`issue=7&name=save_quick_1.sav&key=wrong`), env)).status, 403);
assert.equal((await worker.fetch(put(`issue=8&name=save_quick_1.sav&key=${filedReport.upload}`), env)).status, 403, "A key only fits its own issue");
assert.equal((await worker.fetch(put(`issue=7&name=../x&key=${filedReport.upload}`), env)).status, 400);
assert.equal(calls.length, 0, "Nothing reaches GitHub for a bad save");
r = await worker.fetch(put(`issue=7&name=save_quick_1.sav&key=${filedReport.upload}`), env);
assert.equal(r.status, 204);
assert.equal(calls[1].url, "https://uploads.github.com/repos/owner/reports/releases/5/assets?name=issue-7-save_quick_1.sav");
assert.equal(calls[1].init.headers["Content-Length"], "9");
assert.equal(calls[2].url, "https://api.github.com/repos/owner/reports/issues/7/comments");
assert.match(JSON.parse(calls[2].init.body).body, /^Save: .*issue-7-save_quick_1\.sav/);

calls = [];
r = await worker.fetch(put(`issue=7&name=screenshot-1.jpg&key=${filedReport.upload}`, "jpeg"), env);
assert.equal(r.status, 204);
assert.equal(calls[1].url, "https://uploads.github.com/repos/owner/reports/releases/5/assets?name=issue-7-screenshot-1.jpg");
assert.match(JSON.parse(calls[2].init.body).body, /^Screenshot: .*\n\n!\[screenshot-1\.jpg\]\(/, "A screenshot shows inline");

calls = [];
r = await worker.fetch(put(`issue=7&name=video-2.mp4&key=${filedReport.upload}`, "video"), env);
assert.equal(r.status, 204);
assert.match(JSON.parse(calls[2].init.body).body, /^Video: \[video-2\.mp4\]/);

allowed = false;
assert.equal((await worker.fetch(post({ title: "Gothic crashed", body: "log" }), env)).status, 429);
allowed = true;

// launcher.json: notices for everyone, a verdict only for the asking install or device
const settingsFile = {
  notices: [{ id: "now", text: { en: "Working on it" } }, { id: "old", text: { en: "Gone" }, until: "2000-01-01" }],
  blocked: [{ install: "bad-install", reason: "spam" }, { match: "Pixel 1\\b", reason: "broken GPU" }, { match: "(", reason: "broken entry" }],
};
const githubFetch = globalThis.fetch;
globalThis.fetch = async (url, init) => url.endsWith("/contents/launcher.json") ? Response.json(settingsFile) : githubFetch(url, init);
const fresh = (await import("./worker.js?settings")).default;
const config = async (query) => (await fresh.fetch(new Request(`https://relay.example/config?${query}`), env)).json();
let c = await config("install=good&device=Google%20Pixel%208a");
assert.deepEqual(c.notices.map(n => n.id), ["now"], "An expired notice is gone");
assert.equal(c.blocked, null);
assert.deepEqual(Object.keys(c).sort(), ["blocked", "notices"], "The block list itself never leaves the Worker");
assert.equal((await config("install=bad-install&device=x")).blocked, "spam");
assert.equal((await config("install=good&device=Google%20Pixel%201")).blocked, "broken GPU");
assert.equal((await fresh.fetch(post({ title: "t", body: "b", install: "bad-install" }), env)).status, 403, "A blocked install files nothing");
assert.equal((await fresh.fetch(post({ title: "t", body: "b", install: "good", device: "Google Pixel 8a" }), env)).status, 200);
console.log("ok");
