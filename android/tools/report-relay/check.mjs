// node check.mjs: runs the Worker against a stand-in for GitHub and the rate limit
import assert from "node:assert";
import worker from "./worker.js";

let sent = null;
globalThis.fetch = async (url, init) => {
  sent = { url, init };
  return Response.json({ number: 7 }, { status: 201 });
};
let allowed = true;
const env = { REPO: "owner/reports", GITHUB_TOKEN: "token", LIMIT: { limit: async () => ({ success: allowed }) } };
const post = (body) => new Request("https://relay.example/", { method: "POST", body: typeof body === "string" ? body : JSON.stringify(body) });

let r = await worker.fetch(post({ title: "Gothic crashed", body: "log" }), env);
assert.equal(r.status, 200);
assert.equal((await r.json()).number, 7);
assert.equal(sent.url, "https://api.github.com/repos/owner/reports/issues");
assert.equal(sent.init.headers.Authorization, "Bearer token");
assert.deepEqual(JSON.parse(sent.init.body), { title: "Gothic crashed", body: "log" });

sent = null;
assert.equal((await worker.fetch(post("not json"), env)).status, 400);
assert.equal((await worker.fetch(post({ title: "", body: "log" }), env)).status, 400);
assert.equal((await worker.fetch(post({ title: 1, body: "log" }), env)).status, 400);
assert.equal((await worker.fetch(new Request("https://relay.example/"), env)).status, 404);
assert.equal(sent, null, "Nothing reaches GitHub for a bad request");

r = await worker.fetch(post({ title: "x".repeat(500), body: "y".repeat(70000) }), env);
const filed = JSON.parse(sent.init.body);
assert.equal(filed.title.length, 200);
assert.equal(filed.body.length, 65000);

allowed = false;
assert.equal((await worker.fetch(post({ title: "Gothic crashed", body: "log" }), env)).status, 429);
console.log("ok");
