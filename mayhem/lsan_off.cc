// Leaks are not the bug class this port hunts; ASan and UBSan stay on.
extern "C" int __lsan_is_turned_off() { return 1; }
