#!/usr/bin/env python3
"""Experience provenance, recovery and authority contracts against the real MCP."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
import uuid

from mcp import Client  # consumes the executable argument, as in tests/mcp.py


def trace(run_id, memory="none"):
    events = []
    def event(kind, payload):
        events.append({"id": f"e{len(events)+1:04d}", "at": "2026-11-12T17:00:00Z",
                       "kind": kind, "payload": payload})
        return events[-1]["id"]
    event("mission", {"run_id": run_id, "config": {"memory_policy": memory}})
    def wire(kind, value, server="metis"):
        raw = json.dumps(value)+"\n"
        return event(kind, {"server": server, "wire": raw, "sha256": hashlib.sha256(raw.encode()).hexdigest()})
    wire("mcp_request", {"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "capabilities", "arguments": {}}})
    response = wire("mcp_response", {"jsonrpc": "2.0", "id": 1, "result": {"isError": False, "content": [{"type": "text", "text": "experimental"}]}})
    focus = {"question": "Questão aberta?", "reason": "Capacidade observada", "response_ids": [response]}
    event("initial_focus", focus)
    event("instrument_decision", {"purpose": "support", "why": "Buscar apoio"})
    event("instrument_decision", {"purpose": "alternative", "why": "Confrontar hipótese"})
    event("outcome", {"status": "ABSTENTION", "initial_focus": focus})
    return events


class Experiences(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="noese-experience-")
        self.data = Path(self.temp.name)/"archive"
        self.client = Client(self.data, True)

    def tearDown(self):
        self.client.close()
        self.temp.cleanup()

    def args(self, capture="trace"):
        run_id = str(uuid.uuid4())
        args = {"run_id": run_id, "title": "Experience contract", "capture": capture,
                "producer": "local_harness" if capture == "trace" else "conversation",
                "purpose": "operational_validation" if capture == "trace" else "retrospective",
                "memory_policy": "none" if capture == "trace" else "not_recorded",
                "limitations": "Contract fixture; not scientific validation", "request_id": str(uuid.uuid4())}
        if capture == "trace":
            args["journal"] = self.serialize(trace(run_id))
        else:
            args["report"] = "Relatório <script>não executar</script>; sem respostas brutas."
        return args

    def serialize(self, events):
        return "".join(json.dumps(event, ensure_ascii=False)+"\n" for event in events)

    def error(self, args):
        result = self.client.call("preserve_experience", args)
        self.assertTrue(result["result"]["isError"], result)
        self.assertEqual(self.client.value("experiences")["items"], [])

    def test_trace_recovers_focus_order_wire_bytes_and_export(self):
        args = self.args()
        result = self.client.value("preserve_experience", args)
        again = self.client.value("preserve_experience", args)
        self.assertEqual(result, again)
        recovered = self.client.value("experience", {"id": result["inquiry_id"], "include_events": True})
        self.assertEqual(recovered["integrity"], "VERIFIED")
        self.assertEqual(recovered["experience"]["focus_provenance"], "RECORDED_BEFORE_INVESTIGATION")
        self.assertEqual(recovered["experience"]["human_evaluation"], "PENDING")
        self.assertEqual(recovered["trace"]["events"], trace(args["run_id"]))
        exported = self.client.value("export_inquiry", {"id": result["inquiry_id"]})
        self.assertEqual(bytes.fromhex(exported["originals"][0]["bytes"]), args["journal"].encode())
        self.assertEqual(self.client.value("verify")["technical_integrity"], "VERIFIED")
        self.assertEqual(len(self.client.value("experiences")["items"]), 1)
        summary = self.client.value("experience", {"id": result["inquiry_id"]})
        self.assertFalse(summary["trace"]["events_included"])
        self.assertNotIn("events", summary["trace"])

    def test_secondary_report_never_fabricates_initial_focus_or_raw_events(self):
        args = self.args("secondary_report")
        result = self.client.value("preserve_experience", args)
        recovered = self.client.value("experience", {"id": result["inquiry_id"]})
        self.assertEqual(recovered["report"], args["report"])
        self.assertNotIn("trace", recovered)
        self.assertIsNone(recovered["experience"]["initial_focus"])
        self.assertEqual(recovered["experience"]["focus_provenance"], "RETROSPECTIVE_OR_UNKNOWN")
        self.assertEqual(recovered["assessments"], [])

    def test_retrospective_focus_and_later_evidence_are_rejected(self):
        args = self.args()
        events = trace(args["run_id"])
        events[3], events[4] = events[4], events[3]
        for i, item in enumerate(events, 1): item["id"] = f"e{i:04d}"
        args["journal"] = self.serialize(events)
        self.error(args)
        events = trace(args["run_id"])
        events[3]["payload"]["response_ids"] = ["e9999"]
        args["journal"] = self.serialize(events)
        self.error(args)

    def test_tampered_wire_or_changed_final_focus_are_rejected(self):
        args = self.args()
        events = trace(args["run_id"])
        events[2]["payload"]["wire"] += " "
        args["journal"] = self.serialize(events)
        self.error(args)
        events = trace(args["run_id"])
        events[-1]["payload"]["initial_focus"] = copy.deepcopy(events[3]["payload"])
        events[-1]["payload"]["initial_focus"]["question"] = "Reescrita posterior"
        args["journal"] = self.serialize(events)
        self.error(args)

    def test_declared_no_memory_cannot_hide_archive_queries(self):
        args = self.args()
        events = trace(args["run_id"])
        rpc = {"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "search", "arguments": {"query": "BB-001"}}}
        raw = json.dumps(rpc)+"\n"
        events[1]["payload"].update(server="noese", wire=raw, sha256=hashlib.sha256(raw.encode()).hexdigest())
        events[2]["payload"]["server"] = "noese"
        args["journal"] = self.serialize(events)
        self.error(args)

    def test_read_only_denial_windows_and_reserved_metadata(self):
        args = self.args("secondary_report")
        with Client(self.data) as reader:
            self.assertTrue(reader.call("preserve_experience", args)["result"]["isError"])
        result = self.client.value("preserve_experience", {**args, "window": 2})
        self.assertEqual(self.client.value("experiences", {"window": 0})["items"], [])
        self.assertTrue(self.client.call("experience", {"id": result["inquiry_id"], "window": 0})["result"]["isError"])

    def test_attach_existing_inquiry_keeps_prior_version(self):
        inquiry = self.client.value("create_inquiry", {"title": "Prior record", "request_id": str(uuid.uuid4()), "window": 0})
        args = self.args("secondary_report")
        self.client.value("preserve_experience", {**args, "inquiry_id": inquiry["id"], "window": 2})
        prior = self.client.value("inquiry", {"id": inquiry["id"], "window": 0})
        self.assertNotIn("experience", prior)
        self.assertEqual(prior["title"], "Prior record")
        self.assertEqual(self.client.value("experience", {"id": inquiry["id"]})["inquiry"]["version"], 2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
