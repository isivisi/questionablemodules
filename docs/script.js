// Settings
var DocVersion = "v2.1.5";
var previousVersions = [
    {
        verison: "v2.1.4",
    }
    /*{
        verison: "v2.1.4",
        commit: "ae440e0e43fe738f03537fc4261bc3fdc9fc9ad1"
    }*/
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