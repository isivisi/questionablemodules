// Settings
var DocVersion = "v2.1.6";
var previousVersions = [
    {
        verison: "v2.1.5",
        commit: "8f4f0ea4718606845b24d6cd821247752e57c334"
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