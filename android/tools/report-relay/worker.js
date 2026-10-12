// Gothic Launcher sends problem reports here and this Worker files them as GitHub issues.
// The GitHub token stays a Worker secret, because anything inside the app can be pulled out of the APK.
export default {
  async fetch(request, env) {
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

    const issue = await fetch(`https://api.github.com/repos/${env.REPO}/issues`, {
      method: "POST",
      headers: {
        Authorization: `Bearer ${env.GITHUB_TOKEN}`,
        Accept: "application/vnd.github+json",
        "User-Agent": "opengothic-reports",
        "X-GitHub-Api-Version": "2022-11-28",
      },
      body: JSON.stringify({ title, body }),
    });
    if (!issue.ok) return new Response("GitHub refused the report", { status: 502 });
    return Response.json({ number: (await issue.json()).number });
  },
};
