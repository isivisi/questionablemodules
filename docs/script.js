async function importHTML(file) {
    const resp = await fetch(file);
    const html = await resp.text();
    document.body.insertAdjacentHTML("beforeend", html);
}