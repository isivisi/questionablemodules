// Settings
var DocVersion = "v2.1.13";
var previousVersions = [
    {
        version: "v2.1.12",
        commit: "doc2.1.12"
    },
    {
        version: "v2.1.11",
        commit: "doc2.1.11"
    },
    {
        version: "v2.1.10",
        commit: "doc2.1.10"
    },
    {
        version: "v2.1.9",
        commit: "doc2.1.9"
    },
    {
        version: "v2.1.8",
        commit: "doc2.1.8"
    },
    {
        version: "v2.1.7",
        commit: "doc2.1.7"
    },
    {
        version: "v2.1.6",
        commit: "doc2.1.6"
    },
    {
        version: "v2.1.5",
        commit: "doc2.1.5"
    },
    {
        version: "v2.1.4",
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

async function loadTemplates() {
    await importHTML("templates.html");
    // load templates
    var templates = document.getElementsByTagName("template");
    for (var i = 0; i < templates.length; i++) {
        var templateDef = templates[i];
        var foundTags = document.getElementsByTagName(templateDef.id);
        for (var x = 0; x < foundTags.length; x++) {
            //var newClone = templateDef.content.cloneNode(true);
            //foundTags[x].parentNode.replaceChild(newClone, foundTags[x]);
            foundTags[x].innerHTML = templateDef.innerHTML;
        }
    }
}

// highlight logic for ids
window.addEventListener('load', function () {
    var hash = decodeURIComponent(window.location.hash.slice(1));
    if (hash) {
        var paragraphsWithIds = document.querySelectorAll("p[id]");
        for (var i = 0; i < paragraphsWithIds.length; i++) {
            console.log(hash, paragraphsWithIds[i].id)
            if (hash.includes(paragraphsWithIds[i].id)) {
                window.location.hash = "#" + paragraphsWithIds[i].id;

                paragraphsWithIds[i].classList.add("hashSelected");
            }
        }
    }

});

loadTemplates();