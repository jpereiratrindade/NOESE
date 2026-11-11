// Optional interoperability check using the official TypeScript MCP SDK.
const { Client } = require("@modelcontextprotocol/sdk/client/index.js");
const {
  StdioClientTransport,
} = require("@modelcontextprotocol/sdk/client/stdio.js");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const os = require("node:os");
const { randomUUID } = require("node:crypto");
const root = path.resolve(__dirname, "..");
const data = fs.mkdtempSync(path.join(os.tmpdir(), "noese-sdk-"));
let client;
async function connect(writes) {
  client = new Client({ name: "official-sdk-test", version: "1.0" });
  const args = ["mcp", "--data", data];
  if (writes) args.push("--allow-writes");
  await client.connect(
    new StdioClientTransport({
      command: path.join(root, "build/noese"),
      args,
      stderr: "pipe",
    }),
  );
}
async function call(name, args) {
  const result = await client.callTool({ name, arguments: args });
  assert.equal(result.isError, false, JSON.stringify(result));
  return JSON.parse(result.content[0].text);
}
(async () => {
  await connect(false);
  assert.equal(client.getServerVersion().name, "noese");
  const readTools = await client.listTools();
  assert.ok(readTools.tools.every((t) => t.annotations.readOnlyHint));
  assert.equal((await call("inquiries", {})).items.length, 0);
  await client.close();
  await connect(true);
  const tools = await client.listTools();
  assert.ok(tools.tools.some((t) => t.name === "propose_note"));
  const args = {
    title: "Official SDK interoperability test",
    request_id: randomUUID(),
  };
  const inquiry = await call("create_inquiry", args);
  assert.deepEqual(await call("create_inquiry", args), inquiry);
  const note = await call("propose_note", {
    inquiry_id: inquiry.id,
    text: "Candidate from declared SDK client.",
    basis_unresolved: true,
    limitation: "Synthetic integration test; no scientific claim.",
    request_id: randomUUID(),
  });
  const reference = await call("reference", { id: inquiry.id });
  assert.equal(reference.statements.items[0].id, note.id);
  assert.equal(note.created_by, "mcp:official-sdk-test");
  assert.equal(note.status, "INFERRED_CANDIDATE");
  assert.equal((await call("verify", {})).technical_integrity, "VERIFIED");
  const result = {
    sdk: JSON.parse(
      fs.readFileSync(
        path.resolve(
          path.dirname(
            require.resolve("@modelcontextprotocol/sdk/client/index.js"),
          ),
          "../../../package.json",
        ),
        "utf8",
      ),
    ).version,
    server: client.getServerVersion(),
    checks: [
      "initialize and negotiated protocol",
      "read-only discovery",
      "writes opt-in",
      "idempotent request",
      "proposal with origin",
      "shared reference",
      "technical integrity",
    ],
  };
  fs.mkdirSync(path.join(root, "build/mcp"), { recursive: true });
  fs.writeFileSync(
    path.join(root, "build/mcp/sdk-results.json"),
    JSON.stringify(result, null, 2),
  );
  console.log(JSON.stringify(result, null, 2));
})()
  .catch((e) => {
    console.error(e);
    process.exitCode = 1;
  })
  .finally(async () => {
    if (client) await client.close();
    fs.rmSync(data, { recursive: true, force: true });
  });
