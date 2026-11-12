"use strict";
// Shared responsive navigation; investigation state remains owned by each page.
(() => {
  const menu = document.getElementById("sister-menu");
  const sidebar = document.getElementById("sister-sidebar");
  const close = document.getElementById("sister-nav-close");
  const scrim = document.getElementById("sister-scrim");
  const workspace = document.querySelector(".sister-workspace");
  const narrow = matchMedia("(max-width: 860px)");
  let opened = false;
  function setOpen(value, restoreFocus = false) {
    opened = narrow.matches && value;
    sidebar.classList.toggle("is-open", opened);
    sidebar.inert = narrow.matches && !opened;
    workspace.inert = opened;
    scrim.hidden = !opened;
    menu.setAttribute("aria-expanded", String(opened));
    document.body.classList.toggle("navigation-open", opened);
    if (opened) close.focus();
    else if (restoreFocus) menu.focus();
  }
  menu.addEventListener("click", () => setOpen(!opened));
  close.addEventListener("click", () => setOpen(false, true));
  scrim.addEventListener("click", () => setOpen(false, true));
  document.addEventListener("keydown", (event) => {
    if (!opened) return;
    if (event.key === "Escape") setOpen(false, true);
    if (event.key === "Tab") {
      const items = [...sidebar.querySelectorAll("a[href], button")];
      const first = items[0],
        last = items.at(-1);
      if (event.shiftKey && document.activeElement === first) {
        event.preventDefault();
        last.focus();
      } else if (!event.shiftKey && document.activeElement === last) {
        event.preventDefault();
        first.focus();
      }
    }
  });
  narrow.addEventListener("change", () => setOpen(false));
  setOpen(false);
})();
