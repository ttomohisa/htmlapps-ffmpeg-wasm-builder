// Real-browser PoC smoke test. Uses actual built WASM, not an empty module.
// This test proves one short H.264 MP4 reverse-chunk export is possible;
// it does NOT yet prove reversed frame identity, iOS/Android performance,
// orientation, audio, stitching, or long-video memory stability.
const runner = await BrowserFFmpeg.loadEmbedded({coreJsText, wasmBytes, threading: threadingMode});
const path = "/reverse-chunk.mp4";
let progress = 0;
const result = await runner.run({
  files: [{name:"/workerfs/input.mp4",data:new File([input],"smoke-input.mp4",{type:"video/mp4"}),workerfs:true}],
  outputs: [path],
  args: ["--input","/workerfs/input.mp4","--output",path,
         "--start-time","0.1","--duration","0.4",
         "--width","160","--height","90","--fps","10","--frames","4","--crf","34"],
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
  // Decode → reverse two disjoint segments → remux in reverse chronology.
  // Each runner invocation runs the actual dedicated WASM in a new Worker.
  const createChunk = async (startTime) => {
    const chunk = await runner.run({
      files:[{name:"/workerfs/input.mp4",
        data:new File([input],"smoke-input.mp4",{type:"video/mp4"}),workerfs:true}],
      outputs:["/reverse-chunk.mp4"],
      args:["--input","/workerfs/input.mp4","--output","/reverse-chunk.mp4",
        "--start-time",String(startTime),"--duration","0.2",
        "--width","160","--height","90","--fps","10","--frames","2","--crf","34"]
    });
    if (chunk.exitCode !== 0 || !chunk.files?.length) throw new Error("WASM chunk render failed");
    return chunk.files[0].data;
  };
  const early=await createChunk(0);
  const late=await createChunk(0.2);
  const concat=await runner.run({
    files:[
      {name:"/later.mp4",data:late},
      {name:"/earlier.mp4",data:early}
    ],
    outputs:["/joined.mp4"],
    args:["--concat","--input","/later.mp4","--input","/earlier.mp4",
      "--output","/joined.mp4","--expected-frames","4"],
    onLog:({message})=>append(message)
  });
  const joined=concat.files?.[0]?.data;
  if (concat.exitCode !== 0 || !joined || joined.byteLength < 1000 ||
      String.fromCharCode(...joined.slice(4,8)) !== "ftyp" ||
      !containsAscii(joined,"avc1") || containsAscii(joined,"mp4a"))
    throw new Error("Joined WASM output is not a silent H.264 MP4.");
  // Verify actual browser playback metadata and that both segments survived.
  const combinedUrl=URL.createObjectURL(new Blob([joined],{type:"video/mp4"}));
  try {
    const combinedVideo=document.createElement("video");
    const combinedDuration=await new Promise((resolve,reject)=>{
      const timeout=setTimeout(()=>reject(new Error("Joined MP4 metadata timeout")),8000);
      combinedVideo.onloadedmetadata=()=>{clearTimeout(timeout);resolve(combinedVideo.duration);};
      combinedVideo.onerror=()=>{clearTimeout(timeout);reject(new Error("Joined MP4 is unplayable"));};
      combinedVideo.src=combinedUrl;
    });
    if (!(combinedDuration>=0.25 && combinedDuration<=0.75))
      throw new Error("Reverse join lost/duplicated a segment: duration="+combinedDuration);
    append("joinedDuration="+combinedDuration.toFixed(3)+"/bytes="+joined.byteLength);
    append("reverseJoinedDuration="+combinedDuration.toFixed(3));
  }finally{
    URL.revokeObjectURL(combinedUrl);
  }

  // Ping-pong must produce real forward frames followed by reversed frames,
  // skipping both endpoints on the return to avoid a pause at the turn/loop.
  const pingInput = new File([input], "pingpong-fixture.mp4", {type:"video/mp4"});
  const pingPart = async (direction,start,duration,frames) => {
    const part = await runner.run({
      files:[{name:"/workerfs/input.mp4",data:pingInput,workerfs:true}],
      outputs:["/ping-part.mp4"],
      args:["--input","/workerfs/input.mp4","--output","/ping-part.mp4",
        "--start-time",String(start),"--duration",String(duration),
        "--width","160","--height","90","--fps","10","--frames",String(frames),
        "--crf","34","--direction",direction]
    });
    if (part.exitCode!==0 || !part.files?.[0]?.data?.length)
      throw new Error("Ping-pong "+direction+" chunk failed");
    return part.files[0].data;
  };
  const forward=await pingPart("forward",0,.4,4);
  const backward=await pingPart("reverse",.1,.2,2);  // f2, f1 (no f3 or f0).
  const stitched=await runner.run({
    files:[{name:"/forward.mp4",data:forward},{name:"/backward.mp4",data:backward}],
    outputs:["/pingpong.mp4"],
    args:["--concat","--input","/forward.mp4","--input","/backward.mp4",
      "--output","/pingpong.mp4","--expected-frames","6"]
  });
  const pingBytes=stitched.files?.[0]?.data;
  if(stitched.exitCode!==0 || !pingBytes || pingBytes.byteLength<700 ||
     String.fromCharCode(...pingBytes.slice(4,8))!=="ftyp" ||
     !containsAscii(pingBytes,"avc1") || containsAscii(pingBytes,"mp4a"))
    throw new Error("Ping-pong forward + reverse MP4 is invalid");
  const pingUrl=URL.createObjectURL(new Blob([pingBytes],{type:"video/mp4"}));
  try {
    const pingVideo=document.createElement("video");
    pingVideo.preload="metadata";
    const pingDuration=await new Promise((resolve,reject)=>{
      const timer=setTimeout(()=>reject(new Error("Ping-pong MP4 metadata timeout")),8000);
      pingVideo.onloadedmetadata=()=>{clearTimeout(timer);resolve(pingVideo.duration);};
      pingVideo.onerror=()=>{clearTimeout(timer);reject(new Error("Ping-pong MP4 is unplayable"));};
      pingVideo.src=pingUrl;
    });
    if(!(pingDuration>=.5 && pingDuration<=.75))
      throw new Error("Expected 6 frame ping-pong duration ~=0.6s, got "+pingDuration);
    append("video_reverse_chunk_join_h264_mp4=0.400");
    append("video_pingpong_no_audio="+pingDuration.toFixed(3));
  }finally{URL.revokeObjectURL(pingUrl);}

  // Separate AAC PoC: extract only a bounded selected source interval.
  // The tiny disposable H.264 placeholder is NOT the export video;
  // the later mux milestone must use only the AAC track.
  const aacFile = new File([input],"synthetic-camera-audio.mp4",{type:"video/mp4"});
  const inspect=await runner.run({
    files:[{name:"/workerfs/source.mp4",data:aacFile,workerfs:true}],
    outputs:["/metadata.json"],
    args:["--inspect-audio","--input","/workerfs/source.mp4","--output","/metadata.json"]
  });
  const info=inspect?.files?.[0]?.data;
  if(inspect.exitCode!==0 || !info) throw new Error("Source audio inspection failed.");
  const inspected=JSON.parse(new TextDecoder().decode(info));
  if(!inspected.audio || inspected.audio.codec!=="aac")
    throw new Error("Expected a genuine source AAC stream for the audio smoke test.");
  for(const direction of ["forward","reverse"]){
    const run=await runner.run({
      files:[{name:"/workerfs/source.mp4",data:aacFile,workerfs:true}],
      outputs:["/audio-pass.mp4"],
      args:["--audio-segment","--input","/workerfs/source.mp4",
            "--output","/audio-pass.mp4","--start-time","0.4",
            "--duration","1","--direction",direction],
      onLog:({message})=>append("audio-"+direction+": "+message)
    });
    const bytes=run?.files?.[0]?.data;
    if(run.exitCode!==0 || !bytes || bytes.byteLength<500 ||
       String.fromCharCode(...bytes.slice(4,8))!=="ftyp" ||
       !containsAscii(bytes,"mp4a"))
      throw new Error("Experimental "+direction+" source-audio AAC transform failed.");
    append("real AAC "+direction+" bytes="+bytes.length);
  }
  const makeAudio=async(direction,extras=[])=>{
    const rendered=await runner.run({
      files:[{name:"/workerfs/source.mp4",data:aacFile,workerfs:true}],
      outputs:["/candidate-audio.mp4"],
      args:["--audio-segment","--input","/workerfs/source.mp4",
        "--output","/candidate-audio.mp4","--start-time","0.1",
        "--duration","0.4","--direction",direction,...extras],
      onLog:({message})=>append("audio "+direction+": "+message)
    });
    const b=rendered?.files?.[0]?.data;
    if(rendered.exitCode!==0||!b||!containsAscii(b,"mp4a"))
      throw new Error("Audio "+direction+" candidate missing AAC");
    return b;
  };
  const muxAudio=async(videoData,audioData,count,seconds)=>{
    const result=await runner.run({
      files:[{name:"/v.mp4",data:videoData},{name:"/a.mp4",data:audioData}],
      outputs:["/final-av.mp4"],
      args:["--mux-audio","--video","/v.mp4","--audio","/a.mp4",
        "--output","/final-av.mp4","--expected-frames",String(count),
        "--expected-duration",String(seconds)],
      onLog:({message})=>append("A/V mux: "+message)
    });
    const bytes=result.files?.[0]?.data;
    if(result.exitCode!==0||!bytes||bytes.length<800||
       !containsAscii(bytes,"mp4a")||!containsAscii(bytes,"avc1"))
      throw new Error("Actual H.264+AAC MP4 mux failed for "+seconds+"s");
    const url=URL.createObjectURL(new Blob([bytes],{type:"video/mp4"}));
    try{
      const v=document.createElement("video");
      const d=await new Promise((resolve,reject)=>{
        const t=setTimeout(()=>reject(new Error("A/V preview timeout")),10000);
        v.onloadedmetadata=()=>{clearTimeout(t);resolve(v.duration)};
        v.onerror=()=>{clearTimeout(t);reject(new Error("A/V MP4 unplayable"))};
        v.src=url;
      });
      if(Math.abs(d-seconds)>.15)throw new Error("Unexpected A/V duration "+d);
    }finally{URL.revokeObjectURL(url)}
  };
  await muxAudio(joined,await makeAudio("reverse"),4,.4);
  await muxAudio(pingBytes,await makeAudio("pingpong",["--fps","10","--frames","4"]),6,.6);
  pass("video_reverse_pingpong_actual_AAC_MP4_mux_ST_MT");
} finally {
  URL.revokeObjectURL(url);
  runner.dispose();
}
