///
/// @file
/// @brief eForth - worker proxy to eforth.js (called by eforth.html)
///
Module = {
    mainScriptUrlOrBlob: 'eforth.js',                ///< worker thread (emscriptened C)
    print              : txt=>postMessage(txt)
} ///> WASM print interface => output queue
///
/// worker message pipeline to main thread
///
var forth = null;
    
onmessage = (e)=>{
    console.warn("eforth_vm0.js: handleMessage=", e.data);
    if (e.data?.cmd === 'init') {
        importScripts(Module.mainScriptUrlOrBlob);  /// * load emscripten code
        forth = Module.cwrap('forth', null, ['number', 'string']);
        postMessage({ 'cmd' : 'ready'});
    }
    else {
        var rst = forth(0, e.data);                 ///< process Forth command
        console.log(">>" + rst)
        if (rst) postMessage(rst)                   /// * send response to main
    }
}

