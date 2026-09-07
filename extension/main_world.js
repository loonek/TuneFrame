(() =>
{
  // Builds data for the esp in correct format
  function readNowPlaying()
  {
    const v = document.querySelector("video");
    const m = navigator.mediaSession && navigator.mediaSession.metadata;
    const mp = document.getElementById("movie_player");
    const pr = mp && mp.getPlayerResponse ? mp.getPlayerResponse() : null;
    const len = pr && pr.videoDetails ? parseInt(pr.videoDetails.lengthSeconds, 10) : 0;
    const vid = pr && pr.videoDetails ? pr.videoDetails.videoId : "";
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
      dur: len > 0 ? len : Math.floor(v.duration || 0),
      vid: vid,
      vol: mp && mp.getVolume ? Math.round(mp.getVolume()) : Math.round((v.volume || 0) * 100),
      cover_url: art,
    };
  }

  // Executes esp's commands
  function executeCommand(action, id, kind)
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
      if (kind === "p") mp.loadPlaylist({ playlist: id.replace(/^VL/, "") });
      else if (kind === "b") location.assign("/browse/" + id);
      else mp.loadVideoById(id);
    }
    else if (action === "next")
    {
      document.querySelector(".next-button")?.click();
    }
    else if (action === "prev")
    {
      document.querySelector(".previous-button")?.click();
    }
    else if (action === "search")
    {
      const box = document.querySelector("ytmusic-search-box");
      if (box)
      {
        const btn = box.querySelector(".search-button button");
        if (btn) btn.click();
        const input = box.querySelector("#input");
        if (input)
        {
          input.focus();
          input.select();
        }
      }
    }
  }

  // Extract runs' text, since text is stored in runs tables on innerTube
  function firstRun(x)
  {
    return x && x.runs && x.runs[0] ? x.runs[0].text : "";
  }

  // firstRun takes the first fragment, joinRuns gathers the rest
  function joinRuns(x)
  {
    return x && x.runs ? x.runs.map((r) => r.text).join("") : "";
  }

  // Gets target from navigationEndpoint. 
  // watchEndpoint.videoId -> v, watchPlaylistEndpoint.playlistId -> p, browseEndpoint.browseId -> b
  function itemTarget(nav)
  {
    if (!nav) return { id: "", kind: "" };
    if (nav.watchEndpoint) return { id: nav.watchEndpoint.videoId, kind: "v" };
    if (nav.watchPlaylistEndpoint) return { id: nav.watchPlaylistEndpoint.playlistId, kind: "p" };
    if (nav.browseEndpoint) return { id: nav.browseEndpoint.browseId, kind: "b" };
    return { id: "", kind: "" };
  }

  // Reads an album tile into {title, sub, id, kind}
  function parseTwoRow(it)
  {
    const target = itemTarget(it.navigationEndpoint);
    return { title: firstRun(it.title), sub: joinRuns(it.subtitle), id: target.id, kind: target.kind };
  }

  // Reads a song row into {title, sub, id, kind}
  function parseListRow(it)
  {
    let title = "";
    let sub = "";
    try { title = firstRun(it.flexColumns[0].musicResponsiveListItemFlexColumnRenderer.text); }
    catch (e) {}
    try { sub = joinRuns(it.flexColumns[1].musicResponsiveListItemFlexColumnRenderer.text); }
    catch (e) {}

    if (it.playlistItemData && it.playlistItemData.videoId)
    {
      return { title: title, sub: sub, id: it.playlistItemData.videoId, kind: "v" };
    }
    let nav = it.navigationEndpoint;
    try { if (!nav) nav = it.flexColumns[0].musicResponsiveListItemFlexColumnRenderer.text.runs[0].navigationEndpoint; }
    catch (e) {}
    const target = itemTarget(nav);
    return { title: title, sub: sub, id: target.id, kind: target.kind };
  }

  // Returns an array of shelves. Tries 2 paths, since they can look different.
  function shelvesOf(resp)
  {
    try { return resp.contents.singleColumnBrowseResultsRenderer.tabs[0].tabRenderer.content.sectionListRenderer.contents; }
    catch (e) {}
    try { return resp.continuationContents.sectionListContinuation.contents; }
    catch (e) {}
    return [];
  }

  // Takes the next page token from the same response as the last function. It is used in browse to lookup next shelves
  function contTokenOf(resp)
  {
    try { return resp.contents.singleColumnBrowseResultsRenderer.tabs[0].tabRenderer.content.sectionListRenderer.continuations[0].nextContinuationData.continuation; }
    catch (e) {}
    try { return resp.continuationContents.sectionListContinuation.continuations[0].nextContinuationData.continuation; }
    catch (e) {}
    return null;
  }

  // Adds new shelf, takes the title from the firstRun and goes through the contents. Checks type for each item:
  // album -> parseTwoRow
  // song -> parseListRow
  // Returns title and contents
  function parseShelf(sec)
  {
    const shelf = sec.musicCarouselShelfRenderer || sec.musicImmersiveCarouselShelfRenderer;
    if (!shelf) return null;

    let title = "";
    try { title = firstRun(shelf.header.musicCarouselShelfBasicHeaderRenderer.title); }
    catch (e) {}

    const items = [];
    for (const c of shelf.contents || [])
    {
      let item = null;
      if (c.musicTwoRowItemRenderer) item = parseTwoRow(c.musicTwoRowItemRenderer);
      else if (c.musicResponsiveListItemRenderer) item = parseListRow(c.musicResponsiveListItemRenderer);
      if (item && item.id) items.push(item);
    }
    if (!items.length) return null;
    return { title: title, items: items };
  }

  const FEED_WANT = ["quick picks"];

  // Builds auth headers (SAPISIDHASH + PageId) proving the logged-in user, so YT returns personalized data
  async function authHeaders()
  {
    const headers = { "Content-Type": "application/json" };
    const m = document.cookie.match(/SAPISID=([^;]+)/) || document.cookie.match(/__Secure-3PAPISID=([^;]+)/);
    if (!m) return headers;
    const origin = "https://music.youtube.com";
    const ts = Math.floor(Date.now() / 1000);
    const buf = await crypto.subtle.digest("SHA-1", new TextEncoder().encode(ts + " " + m[1] + " " + origin));
    const hash = Array.from(new Uint8Array(buf)).map((b) => b.toString(16).padStart(2, "0")).join("");
    headers["Authorization"] = "SAPISIDHASH " + ts + "_" + hash;
    headers["X-Goog-AuthUser"] = "0";
    const pageId = window.ytcfg && window.ytcfg.get ? window.ytcfg.get("DELEGATED_SESSION_ID") : null;
    if (pageId) headers["X-Goog-PageId"] = pageId;
    return headers;
  }

  // Sends HTTP request to yt
  async function browse(key, headers, body, params)
  {
    const res = await fetch("/youtubei/v1/browse?prettyPrint=false&key=" + key + (params || ""), {
      method: "POST",
      headers: headers,
      credentials: "include", // Uses login cookies
      body: JSON.stringify(body),
    });
    return res.json();
  }

  // Parses user's listening history, removes duplicates and creates "recently listened"
  function parseHistory(resp)
  {
    let sl;
    try { sl = resp.contents.singleColumnBrowseResultsRenderer.tabs[0].tabRenderer.content.sectionListRenderer.contents; }
    catch (e) { return null; }

    const items = [];
    const seen = {};
    const eat = (shelf) =>
    {
      for (const c of shelf.contents || [])
      {
        if (!c.musicResponsiveListItemRenderer) continue;
        const it = parseListRow(c.musicResponsiveListItemRenderer);
        if (it.id && !seen[it.id]) { seen[it.id] = 1; items.push(it); }
      }
    };
    for (const sec of sl)
    {
      if (sec.musicShelfRenderer) eat(sec.musicShelfRenderer);
      else if (sec.itemSectionRenderer)
      {
        for (const c of sec.itemSectionRenderer.contents || []) if (c.musicShelfRenderer) eat(c.musicShelfRenderer);
      }
    }
    if (!items.length) return null;
    return { title: "Ostatnio odtwarzane", items: items.slice(0, 20) };
  }

  // Looks for "quick picks" from the feed, and adds the "recently listened" feed
  async function fetchFeed()
  {
    const key = window.ytcfg && window.ytcfg.get ? window.ytcfg.get("INNERTUBE_API_KEY") : null;
    const ctx = window.ytcfg && window.ytcfg.get ? window.ytcfg.get("INNERTUBE_CONTEXT") : null;
    if (!key || !ctx) return [];
    const headers = await authHeaders();

    const out = [];
    const found = {};
    let resp = await browse(key, headers, { context: ctx, browseId: "FEmusic_home" });
    for (let page = 0; page < 8; page++)
    {
      for (const sec of shelvesOf(resp))
      {
        const parsed = parseShelf(sec);
        if (!parsed) continue;
        const name = parsed.title.toLowerCase();
        if (FEED_WANT.includes(name) && !found[name]) found[name] = parsed;
      }
      if (FEED_WANT.every((w) => found[w])) break;
      const token = contTokenOf(resp);
      if (!token) break;
      resp = await browse(key, headers, { context: ctx }, "&ctoken=" + token + "&continuation=" + token + "&type=next");
    }
    for (const w of FEED_WANT) if (found[w]) out.push(found[w]);

    try
    {
      const hist = parseHistory(await browse(key, headers, { context: ctx, browseId: "FEmusic_history" }));
      if (hist) out.push(hist);
    }
    catch (e) {}

    return out;
  }

  window.ytmcFeed = fetchFeed;

  // Cyclically sends readNowPlaying() upwards
  setInterval(() =>
  {
    window.postMessage({ source: "ytmc", dir: "up", payload: readNowPlaying() }, "*");
  }, 300);

  // Receives ESP commands; runs "feed" and returns its result upwards, otherwise executes the command
  window.addEventListener("message", (e) =>
  {
    if (e.source !== window) return;
    const d = e.data;
    if (!d || d.source !== "ytmc" || d.dir !== "down") return;
    if (!d.payload) return;
    if (d.payload.t === "cmd" && d.payload.a === "feed")
    {
      fetchFeed().then((sections) =>
      {
        window.postMessage({ source: "ytmc", dir: "up", payload: { t: "feed", sections: sections } }, "*");
      });
    }
    else if (d.payload.t === "cmd")
    {
      executeCommand(d.payload.a, d.payload.id, d.payload.k);
    }
  });
})();
