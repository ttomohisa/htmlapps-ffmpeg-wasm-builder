// All non-reverse multi-thread public-libav runners need an explicit cleanup
// hook for Emscripten's nested pthread pool before their outer Worker exits.
// The reverse profile retains its existing pinned equivalent for provenance.
if (typeof PThread !== "undefined" && !ENVIRONMENT_IS_PTHREAD) {
  Module["shutdownPthreadWorkers"] = () => {
    PThread.terminateAllThreads();
  };
}
