/* Hero panel selection + sprite randomization. Enhancement only: the page
   is fully usable without JS. */
(function () {
  "use strict";

  var SPRITES = {
    ki1: [
      { file: "ki1_fulgore.png", name: "FULGORE" },
      { file: "ki1_orchid.png", name: "ORCHID" },
      { file: "ki1_jago.png", name: "JAGO" },
      { file: "ki1_sabrewulf.png", name: "SABREWULF" },
      { file: "ki1_spinal.png", name: "SPINAL" }
    ],
    ki2: [
      { file: "ki2_fulgore.png", name: "FULGORE" },
      { file: "ki2_jago.png", name: "JAGO" },
      { file: "ki2_orchid.png", name: "ORCHID" },
      { file: "ki2_glacius.png", name: "GLACIUS" },
      { file: "ki2_tusk.png", name: "TUSK" }
    ]
  };

  function randomize(game) {
    var pool = SPRITES[game];
    var pick = pool[Math.floor(Math.random() * pool.length)];
    var img = document.getElementById("sprite-" + game);
    var cap = document.getElementById("name-" + game);
    if (!img || !cap) return;
    img.src = "assets/img/" + pick.file;
    img.alt = pick.name.charAt(0) + pick.name.slice(1).toLowerCase() + " sprite";
    cap.textContent = pick.name;
  }

  var arena = document.querySelector(".arena");

  function select(game) {
    if (arena) arena.dataset.selected = game;
  }

  ["ki1", "ki2"].forEach(function (game) {
    randomize(game);
    var panel = document.getElementById("panel-" + game);
    if (!panel) return;
    panel.addEventListener("pointerenter", function () { select(game); });
    panel.addEventListener("focus", function () { select(game); });
  });

  document.addEventListener("keydown", function (ev) {
    if (ev.key === "ArrowLeft") select("ki1");
    if (ev.key === "ArrowRight") select("ki2");
  });

  if (window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
    var video = document.querySelector(".swap-video");
    if (video) {
      video.removeAttribute("autoplay");
      video.pause();
      video.controls = true;
    }
  }
})();
