// Translates between the site's postMessage and the background (service worker).
// The service worker can be recycled while the tab stays open, which kills the
// port; reconnecting on disconnect keeps control alive without a page reload.

let port = null;

// Opens the port to the background and re-opens it whenever it drops
function connect()
{
  port = chrome.runtime.connect();

  port.onMessage.addListener((data) =>
  {
    let cmd;
    try { cmd = JSON.parse(data); } catch (e) { return; }
    window.postMessage({ source: "ytmc", dir: "down", payload: cmd }, "*");
  });

  port.onDisconnect.addListener(() =>
  {
    port = null;
    setTimeout(connect, 1000);   // service worker went away; rebuild the pipe
  });
}

connect();

window.addEventListener("message", (e) =>
{
  if (e.source !== window) return;
  const d = e.data;
  if (!d || d.source !== "ytmc" || d.dir !== "up") return;
  if (!port) connect();
  try { port.postMessage(d.payload); }
  catch (err) { port = null; connect(); }
});
