#!/usr/bin/env python3
"""Contract tests; deterministic provider is not an investigation evaluation."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import threading
import time
from types import SimpleNamespace
import unittest
from unittest.mock import patch
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("bb", ROOT/"scripts/bb_harness.py")
bb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bb)


def fake_mcp():
    for line in sys.stdin:
        request = json.loads(line)
        if "id" not in request:
            continue
        method = request["method"]
        if method == "initialize":
            value = {"protocolVersion": bb.PROTOCOL}
        elif method == "tools/list":
            value = {"tools": [{"name": name, "description": "Deterministic test tool",
                "inputSchema": bb.obj({}) if name == "capabilities" else bb.obj({"query": bb.TEXT}),
                "annotations": {"readOnlyHint": True}}
                for name in ("capabilities", "scientific_catalog")]}
        else:
            tool = request["params"]["name"]
            query = request["params"]["arguments"].get("query", "")
            if query == "slow":
                time.sleep(2)
            text = "capacidade experimental" if tool == "capabilities" else "valor observado 10; alternativa: variação paramétrica"
            if query == "oversize":
                text *= 1000
            value = {"isError": query == "failure", "content": [{"type": "text", "text": text}]}
        print(json.dumps({"jsonrpc": "2.0", "id": request["id"], "result": value}), flush=True)


if "--fake-mcp" in sys.argv:
    fake_mcp()
    raise SystemExit(0)


class Contracts(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="bb-harness-test-")
        self.path = Path(self.temp.name)
        self.config = bb.configuration(SimpleNamespace(model="test-model", window=2,
            data=self.path/"archive", metis=ROOT.parent/"METIS"))
        self.config["servers"]["metis"] = {"command": [sys.executable, str(Path(__file__).resolve()), "--fake-mcp"],
                                             "allow": ["capabilities", "scientific_catalog"]}
        self.config["servers"]["noese"]["allow"] = ["capabilities"]
        self.config["limits"]["total_seconds"] = 30
        self.journal = bb.Journal(self.path/"run")
        self.journal.add("mission", {"run_id": "986e0cf8-0544-4ba8-b4d0-e41bb15eebbb", "config": self.config})
        self.harness = bb.Harness(self.config, self.journal)

    def tearDown(self):
        self.temp.cleanup()

    def action(self, h, query="support"):
        return {"initial_focus": {"question": "O resultado é sensível ao parâmetro?",
                  "reason": "Capacidade exploratória observada", "response_ids": list(h.responses)[:1]},
                "decision": "Confrontar duas interpretações", "ready_to_finish": True,
                "calls": [{"server": "metis", "tool": "scientific_catalog", "arguments": {"query": q},
                           "why": "Examinar evidência e variação", "purpose": p}
                          for q, p in [(query, "support"), ("alternative", "alternative")]]}

    def final(self, h):
        refs = list(h.responses)
        return {"outcome": "candidate", "summary": "Sensibilidade merece exame",
                "claims": [{"claim": "Um valor foi observado",
                            "evidence": [{"response_id": refs[-2], "content_index": 0,
                                          "quote": "valor observado 10"}], "scope_limit": "Não valida resiliência"}],
                "alternatives": [{"interpretation": "Variação paramétrica", "response_ids": [refs[-1]],
                                  "assessment": "Permanece plausível"}],
                "limitations": ["Provedor determinístico"], "attention_value": "Pendente de avaliação"}

    def execute(self, action=None, final=None):
        def model(h, schema, instruction):
            value = (action or self.action)(h) if "initial_focus" in schema["properties"] else (final or self.final)(h)
            bb.validate(value, schema)
            return value
        with patch.object(bb.Harness, "model", model):
            return self.harness.run()

    def test_candidate_focus_precedes_consults_and_human_judgment_pending(self):
        result = self.execute()
        self.assertEqual(result["status"], "CANDIDATE")
        self.assertEqual(result["checks"]["pertinence"], "PENDING_HUMAN")
        self.assertEqual(result["checks"]["scientific_source_integrity"], "NOT_ESTABLISHED")
        kinds = [e["kind"] for e in self.journal.events]
        focus = kinds.index("initial_focus")
        consults = [i for i, k in enumerate(kinds) if k == "instrument_decision"]
        self.assertLess(focus, consults[2])
        self.assertEqual(len(result["checks"]["verified_quotes"]), 1)

    def test_untrusted_instruction_cannot_enable_write(self):
        def action(h):
            value = self.action(h)
            value["calls"][0]["tool"] = "propose_investigation_note"
            return value
        result = self.execute(action=action)
        self.assertEqual(result["status"], "PROTOCOL_REJECTED")
        requests = [e["payload"]["wire"] for e in self.journal.events if e["kind"] == "mcp_request"]
        self.assertFalse(any('propose_investigation_note' in r for r in requests))
        self.assertEqual(result["calls"], 2)

    def test_bad_arguments_rejected_before_execution(self):
        def action(h):
            value = self.action(h)
            value["calls"][0]["arguments"]["query"] = 17
            return value
        self.assertEqual(self.execute(action=action)["status"], "PROTOCOL_REJECTED")
        self.assertEqual(self.harness.calls, 2)

    def test_fabricated_quote_rejects_final_candidate(self):
        def final(h):
            value = self.final(h)
            value["claims"][0]["evidence"][0]["quote"] = "valor comprovado 99"
            return value
        self.assertEqual(self.execute(final=final)["status"], "CANDIDATE_REJECTED")

    def test_alternative_must_reference_a_confrontation_call(self):
        def final(h):
            value = self.final(h)
            value["alternatives"][0]["response_ids"] = [list(h.responses)[0]]
            return value
        self.assertEqual(self.execute(final=final)["status"], "CANDIDATE_REJECTED")

    def test_operational_failure_is_not_abstention(self):
        self.assertEqual(self.execute(action=lambda h: self.action(h, "failure"))["status"], "OPERATIONAL_FAILURE")
        self.assertTrue(any(e["kind"] == "mcp_response" and '"isError": true' in e["payload"]["wire"]
                            for e in self.journal.events))

    def test_abstention_is_preserved(self):
        def final(h):
            value = self.final(h)
            value.update(outcome="abstention", claims=[], summary="Apoio insuficiente")
            return value
        result = self.execute(final=final)
        self.assertEqual(result["status"], "ABSTENTION")
        self.assertEqual(result["checks"]["response_quote_integrity"], "NOT_APPLICABLE")

    def test_call_byte_and_time_budgets(self):
        self.config["limits"]["max_calls"] = 2
        self.assertEqual(self.execute()["status"], "BUDGET_EXHAUSTED")
        self.assertEqual(self.harness.calls, 2)

    def test_oversized_response_not_inserted_into_context(self):
        result = self.execute(action=lambda h: self.action(h, "oversize"))
        self.assertEqual(result["status"], "BUDGET_EXHAUSTED")
        self.assertEqual(len(self.harness.responses), 2)

    def test_slow_mcp_is_bounded_and_closed(self):
        self.config["limits"]["mcp_seconds"] = .15
        # Initialization uses the same budget; Python import may itself exceed it.
        start = time.monotonic()
        result = self.execute(action=lambda h: self.action(h, "slow"))
        self.assertEqual(result["status"], "OPERATIONAL_FAILURE")
        self.assertLess(time.monotonic()-start, 4)
        self.assertTrue(all(c.process.poll() is not None for c in self.harness.clients.values()))

    def test_persistence_recovery_and_idempotency_without_new_inference(self):
        self.assertEqual(self.execute()["status"], "CANDIDATE")
        command = self.config["servers"]["noese"]["command"]+["--allow-writes"]
        original = (self.journal.path/"events.jsonl").read_bytes()
        first = bb.persist(self.journal.path, command)
        second = bb.persist(self.journal.path, command)
        self.assertEqual(first, second)
        self.assertEqual(first["status"], "RECOVERED")
        self.assertEqual(original, (self.journal.path/"events.jsonl").read_bytes())
        self.assertEqual(first["sources_checked"], len(self.journal.events))

    def test_incomplete_journal_cannot_be_persisted_as_finished(self):
        with self.assertRaises(bb.Rejected):
            bb.persist(self.journal.path, self.config["servers"]["noese"]["command"])

    def test_disabled_memory_is_enforced_before_dispatch(self):
        self.harness.config["memory_policy"] = "none"
        self.harness.catalog["noese"] = {"search": {"inputSchema": bb.obj({"query": bb.TEXT})}}
        with self.assertRaises(bb.Rejected):
            self.harness.consult("noese", "search", {"query": "BB-001"}, "Tentativa indevida", "orientation")
        self.assertEqual(self.harness.calls, 0)

    def test_provider_grammar_compatibility_does_not_relax_local_contract(self):
        schema = bb.action_schema(True)
        grammar = bb.provider_schema(schema)
        self.assertNotIn('"maxLength"', json.dumps(grammar))
        self.assertIn('"minLength"', json.dumps(grammar))
        self.assertEqual(grammar["properties"]["decision"]["minLength"], 1)
        self.assertIn('"maxLength"', json.dumps(schema))
        self.assertEqual(grammar["additionalProperties"], False)
        with self.assertRaises(bb.Rejected):
            bb.validate("x"*4001, bb.TEXT)
        with self.assertRaises(bb.Rejected):
            bb.validate("", bb.TEXT)

    def test_http_error_body_is_preserved_without_retry(self):
        class Provider(BaseHTTPRequestHandler):
            def do_POST(self):
                self.server.calls += 1
                self.rfile.read(int(self.headers["Content-Length"]))
                self.send_response(400)
                self.end_headers()
                self.wfile.write(b'{"error":"failed to parse grammar"}')
            def log_message(self, *_):
                pass
        server = ThreadingHTTPServer(("127.0.0.1", 0), Provider)
        server.calls = 0
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        self.config["ollama"] = f"http://127.0.0.1:{server.server_port}"
        try:
            result = self.harness.run()
            self.assertEqual(result["status"], "OPERATIONAL_FAILURE")
            errors = [e for e in self.journal.events if e["kind"] == "model_http_error"]
            self.assertEqual(len(errors), 1)
            self.assertEqual(errors[0]["payload"]["status"], 400)
            self.assertEqual(errors[0]["payload"]["body"], '{"error":"failed to parse grammar"}')
            self.assertEqual(server.calls, 1)
        finally:
            server.shutdown()
            server.server_close()
            thread.join()

    def test_real_http_provider_contract_and_private_reasoning_exclusion(self):
        owner = self
        class Provider(BaseHTTPRequestHandler):
            def do_POST(self):
                data = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                self.server.requests.append(data)
                h = owner.harness
                value = owner.action(h) if len(self.server.requests) == 1 else owner.final(h)
                raw = bb.encode({"model": "test-model", "done": True, "done_reason": "stop",
                    "eval_count": 100, "message": {"role": "assistant", "content": json.dumps(value),
                                                  "thinking": "private-reasoning-must-not-be-stored"}})
                self.send_response(200)
                self.end_headers()
                self.wfile.write(raw)
            def log_message(self, *_):
                pass
        server = ThreadingHTTPServer(("127.0.0.1", 0), Provider)
        server.requests = []
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        self.config["ollama"] = f"http://127.0.0.1:{server.server_port}"
        try:
            result = self.harness.run()
            self.assertEqual(result["status"], "CANDIDATE")
            self.assertEqual(result["generated_tokens"], 200)
            self.assertEqual(len(server.requests), 2)
            self.assertFalse(server.requests[0]["think"])
            self.assertNotIn(b"private-reasoning-must-not-be-stored", (self.journal.path/"events.jsonl").read_bytes())
        finally:
            server.shutdown()
            server.server_close()
            thread.join()


if __name__ == "__main__":
    unittest.main(verbosity=2)
