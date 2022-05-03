var m = function (f, d, g) {
    d = document;
    g = d.createElement("p");
    g.innerHTML = f;
    f = d.createDocumentFragment();
    while (d = g.firstChild) {
        f.appendChild(d)
    }
    return f
};
var $ = function (d, c) {
    d = d.match(/^(\W)?(.*)/);
    return (c || document)["getElement" + (d[1] ? d[1] == "#" ? "ById" : "sByClassName" : "sByTagName")](d[2])
};
var j = function (b) {
    for (b = 0; b < 4; b++) {
        try {
            return b ? new ActiveXObject([, "Msxml2", "Msxml3", "Microsoft"][b] + ".XMLHTTP") : new XMLHttpRequest
        }
        catch (c) {}
    }
};

function ajaxReq(h, a, g, c) {
    var f = j();
    f.open(h, a, true);
    var d = setTimeout(function () {
        f.abort();
        console.log("XHR abort:", h, a);
        f.status = 599;
        f.responseText = "request time-out"
    }, 9000);
    f.onreadystatechange = function () {
        if (f.readyState != 4) {
            return
        }
        clearTimeout(d);
        if (f.status >= 200 && f.status < 300) {
            g(f.responseText)
        }
        else {
            console.log("XHR ERR :", h, a, "->", f.status, f.responseText, f);
            c(f.status, f.responseText)
        }
    };
    try {
        f.send()
    }
    catch (b) {
        console.log("XHR EXC :", h, a, "->", b);
        c(599, b)
    }
}

function dispatchJson(f, d, c) {
    var a;
    try {
        a = JSON.parse(f)
    }
    catch (b) {
        console.log("JSON parse error: " + b + ". In: " + f);
        c(500, "JSON parse error: " + b);
        return
    }
    d(a)
}

function ajaxJson(d, a, c, b) {
    ajaxReq(d, a, function (f) {
        dispatchJson(f, c, b)
    }, b)
}

function ajaxSpin(d, a, c, b) {
    document.getElementById("spinner")
    document.getElementById("spinner").removeAttribute("hidden");
    ajaxReq(d, a, function (f) {
        document.getElementById("spinner").setAttribute("hidden", "");
        c(f)
    }, function (f, g) {
        document.getElementById("spinner").setAttribute("hidden", "");
        b(f, g)
    })
}

function ajaxJsonSpin(d, a, c, b) {
    ajaxSpin(d, a, function (f) {
        dispatchJson(f, c, b)
    }, b)
}