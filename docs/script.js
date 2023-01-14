async function importHTML(file) {
    const resp = await fetch(file);
    const html = await resp.text();
    document.body.insertAdjacentHTML("beforeend", html);
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