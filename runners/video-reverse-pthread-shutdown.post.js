// Included only in the isolated video-reverse pthread variant via --post-js.
// Emscripten's generated PThread pool is scoped inside createFFmpegCore;
// terminating only its outer JS Worker can leave nested pthread workers alive.
// Do not export it for ST or for any other profile.
if (typeof PThread !== "undefined" && !ENVIRONMENT_IS_PTHREAD) {
  Module["shutdownPthreadWorkers"] = () => {
    PThread.terminateAllThreads();
  };
}
