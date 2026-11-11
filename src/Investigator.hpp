#pragma once
#include "Service.hpp"
#include <atomic>
#include <condition_variable>
#include <thread>
// Reads only authorized archive objects. Inference has no tools, shell, or remote connector.
class Investigator {
  Service &service;
  std::mutex state_mutex;
  std::condition_variable_any wake;
  json state = {{"phase", "idle"}};
  std::jthread worker;
  json configuration();
  json snapshot(int window, const json &config);
  json infer(const json &config, const std::string &instruction, const json &input,
             const json &schema);
  json validate_findings(const json &draft, const json &snapshot, json &rejected);
  void phase(const std::string &name, int window, const std::string &message = "");

public:
  explicit Investigator(Service &s) : service(s) {}
  void start();
  ~Investigator();
  json settings();
  json configure(json input);
  json view(int window);
  json once(int window, bool force = false);
  json feedback(const std::string &id, json input, int window);
};
