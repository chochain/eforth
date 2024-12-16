///
/// @file
/// @brief eForth - worker proxy to eforth.js (called by eforth.html)
///
Module = { print: txt=>postMessage(txt) } ///> WASM print interface => output queue
///
/// worker message pipeline to main thread
///
var forth = null;
    
onmessage = (e)=>{
    console.warn("worker eforth_vm0.js handleMessage=", e.data);
    if (e.data?.cmd === 'load') {
        importScripts('eforth.js');    ///> load js emscripten created
        forth = Module.cwrap('forth', null, ['number', 'string']);
        postMessage({ 'cmd' : 'ready'});
    }
    else {
        var rst = forth(0, e.data);
        console.log(">>" + rst)
        if (rst) postMessage(rst)      /// * send back response from Forth outer interpreter
    }
}

