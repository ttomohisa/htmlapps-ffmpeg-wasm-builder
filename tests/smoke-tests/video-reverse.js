// Real-browser PoC smoke test. Uses actual built WASM, not an empty module.
// This test proves one short H.264 MP4 reverse-chunk export is possible;
// it does NOT yet prove reversed frame identity, iOS/Android performance,
// orientation, audio, stitching, or long-video memory stability.
const runner = await BrowserFFmpeg.loadEmbedded({coreJsText, wasmBytes});
const path = "/reverse-chunk.mp4";
let progress = 0;
const result = await runner.run({
  files: [{name:"/workerfs/input.mp4",data:new File([input],"smoke-input.mp4",{type:"video/mp4"}),workerfs:true}],
  outputs: [path],
  args: ["--input","/workerfs/input.mp4","--output",path,
         "--start-time","0.1","--duration","0.4",
         "--width","160","--height","90","--fps","10","--crf","34"],
  onProgress: value => { progress = Math.max(progress,value); append("progress="+value.toFixed(3)); },
  onLog: ({message}) => append(message)
});
if (result.exitCode !== 0 || result.files?.length !== 1) throw new Error("Reverse chunk runner did not complete");
const output = result.files[0].data;
if (output.byteLength < 700 || String.fromCharCode(...output.slice(4,8)) !== "ftyp") {
  throw new Error("Reverse output does not appear to be an MP4");
}
if (!containsAscii(output,"avc1")) throw new Error("Reverse output lacks H.264 video");
if (containsAscii(output,"mp4a")) throw new Error("Experimental video-only chunk unexpectedly includes AAC");
if (progress < 0.99) throw new Error("Reverse chunk progress never finished");
const url = URL.createObjectURL(new Blob([output],{type:"video/mp4"}));
try {
  const video = document.createElement("video");
  video.preload = "metadata";
  const duration = await new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>reject(new Error("MP4 metadata timeout")),8000);
    video.onloadedmetadata=()=>{clearTimeout(timer);resolve(video.duration);};
    video.onerror=()=>{clearTimeout(timer);reject(new Error("Browser cannot play reversed chunk"));};
    video.src=url;
  });
  if (!(duration >= 0.2 && duration <= 0.75)) {
    throw new Error("Unexpected reverse chunk duration "+duration);
  }
  pass("video_reverse_chunk_h264_mp4="+duration.toFixed(3));
} finally {
  URL.revokeObjectURL(url);
  runner.dispose();
}
