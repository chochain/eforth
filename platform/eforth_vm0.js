///
/// @file
/// @brief eForth - worker proxy to eforth.js (called by eforth.html)
///
Module = { print: txt=>postMessage(txt) } ///> WASM print interface => output queue

importScripts('eforth.js')                ///> load js emscripten created
///
/// worker message pipeline to main thread
///
/// @note: serialization, i.e. structuredClone(), is slow (24ms)
///        so prebuild a transferable object is much faster (~5ms)
///
const forth = Module.cwrap('forth', null, ['number', 'string'])

self.onmessage = function(e) {            ///> worker input message queue
    console.log("<<" + JSON.stringify(e.data))
    if (typeof e.data == 'object') return
    
    let rst = forth(0, e.data)            /// * Forth outer interpreter
    console.log(">>" + rst)
    
    if (rst) postMessage(rst)             /// * send back response from Forth outer interpreter
}

