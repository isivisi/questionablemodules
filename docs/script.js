// Settings
var DocVersion = "v2.1.6";
var previousVersions = [
    {
        verison: "v2.1.5",
        commit: "doc2.1.5"
    },
    {
        verison: "v2.1.4",
    }
]

async function importHTML(file, parent=undefined) {
    const resp = await fetch(file);
    const html = await resp.text();
    if (parent == undefined) document.body.insertAdjacentHTML("beforeEnd", html);
    else parent.insertAdjacentHTML("beforeEnd", html)
    runEmbeddedScripts(html);
}

function runEmbeddedScripts(text) {
    var root = document.createElement('root');
    root.innerHTML = text;

    var scripts = root.getElementsByTagName('script');
    
    for (var i = 0; i < scripts.length; i++) {
        eval(scripts[i].innerHTML);
    }
}

// highlight logic for ids
window.addEventListener('load', function () {
    var hash = window.location.hash.slice(1);
    if (hash) {
        var paragraphsWithIds = document.querySelectorAll("p[id]");
        for (var i = 0; i < paragraphsWithIds.length; i++) {
            if (hash.includes(paragraphsWithIds[i].id)) {
                window.location.hash = "#" + paragraphsWithIds[i].id;

                paragraphsWithIds[i].classList.add("hashSelected");
            }
        }
    } 
});