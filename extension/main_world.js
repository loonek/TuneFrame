(() =>
{
  function readNowPlaying()
  {
    const v = document.querySelector("video");
    const m = navigator.mediaSession && navigator.mediaSession.metadata;
    const mp = document.getElementById("movie_player");
    if (!v || !m) return { t: "np", status: "none" };

    const art = m.artwork && m.artwork.length
      ? m.artwork[m.artwork.length - 1].src
      : "";

    return {
      t: "np",
      title: m.title || "",
      artist: m.artist || "",
      status: v.paused ? "paused" : "playing",
      pos: Math.floor(v.currentTime || 0),
      dur: Math.floor(v.duration || 0),
      vol: mp && mp.getVolume ? Math.round(mp.getVolume()) : Math.round((v.volume || 0) * 100),
      cover_url: art,
    };
  }

  function executeCommand(action, id)
  {
    const v = document.querySelector("video");
    const mp = document.getElementById("movie_player");
    if (!v) return;

    if (action === "playpause")
    {
      v.paused ? v.play() : v.pause();
    }
    else if (action === "vol_up")
    {
      mp.setVolume(Math.min(100, Math.floor(mp.getVolume() / 10) * 10 + 10));
    }
    else if (action === "vol_down")
    {
      mp.setVolume(Math.max(0, Math.ceil(mp.getVolume() / 10) * 10 - 10));
    }
    else if (action === "play" && id)
    {
      mp.loadVideoById(id);
    }
    else if (action === "next")
    {
      document.querySelector(".next-button")?.click();
    }
    else if (action === "prev")
    {
      document.querySelector(".previous-button")?.click();
    }
  }

  setInterval(() =>
  {
    window.postMessage({ source: "ytmc", dir: "up", payload: readNowPlaying() }, "*");
  }, 300);

  window.addEventListener("message", (e) =>
  {
    if (e.source !== window) return;
    const d = e.data;
    if (!d || d.source !== "ytmc" || d.dir !== "down") return;
    if (d.payload && d.payload.t === "cmd") executeCommand(d.payload.a, d.payload.id);
  });
})();
