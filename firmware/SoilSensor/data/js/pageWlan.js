class wlanClientList {
    constructor(array, limit) {
        this.clientDatabase = array;
        this.clientDatabaseLimit = limit;
        this.changed = false;
    }

    init() {
        var table = document.getElementById("wlan-client-network-db").getElementsByTagName("tbody")[0];
        var i;
        for (i = 0; i < this.clientDatabase.length; i++) {
            this.addElement(this.clientDatabase[i]);
        }
        var self = this;
        var button = document.getElementById("wlan-client-scan-button");
        button.onclick = function() {
            self.wlanClientScan(self);
        };
    }

    addCallback(networkSsid, networkPsk) {
        if(this.clientDatabase.length >= this.clientDatabaseLimit) {
            alert("Database limit of " + this.clientDatabaseLimit + " networks reached");
            return false;
        }
        if(networkSsid == undefined)
            return false;

        if(networkPsk == undefined)
            networkPsk = "";

        if(this.clientDatabase.findIndex(x => x.ssid == networkSsid) >= 0) {
            alert("Network \"" + networkSsid + "\" already in database.");
            return false;
        }
        var newClientNetwork = {
            "ssid":"",
            "psk": "",
            "status": 0
        }
        newClientNetwork.ssid = networkSsid;
        newClientNetwork.psk = networkPsk;
        this.addElement(newClientNetwork);
        this.clientDatabase.push(newClientNetwork);
        console.info("Added new client network \"" + networkSsid + "\" to database");
        this.changed = true;
        return true;
    }

    connectNetwork(networkSsid) {
        var req = "wifiConnect?ssid=" + encodeURIComponent(networkSsid);
        ajaxJson("GET", req, function (c) {
            if (c.result = true) {
                console.log(c);
            }
            else {
                console.error("Connecting to " + networkSsid + " failed." + c.error);
                closeDialog();
            }
        }, function (b, a) {
            console.error("Connecting to " + networkSsid + " failed.");
            closeDialog();
        })
    }

    addElement(newClientNetwork) {
        if(newClientNetwork == undefined) return;

        var table = document.getElementById("wlan-client-network-db").getElementsByTagName("tbody")[0];
        var self = this;
        var tr = document.createElement('tr');
        var td = document.createElement('td');
        td.appendChild(document.createTextNode(newClientNetwork.ssid));
        td.style.cursor = "pointer";
        var bgColor = tr.style.backgroundColor;
        td.onmouseover = function() {
            this.parentElement.style.backgroundColor = "#c2c2c2";
        }
        td.onmouseleave = function() { 
            this.parentElement.style.backgroundColor = bgColor;
        }
        td.onclick = function() {
            var connectSsid = this.innerHTML;
            var elementIndex = self.clientDatabase.findIndex(x => x.ssid == newClientNetwork.ssid);
            if(elementIndex >= 0) {
                if(Object.keys(self.clientDatabase[elementIndex].psk).length > 0) {
                    self.connectNetwork(connectSsid);
                }
                else {
                    var dialogDiv = document.createElement("div");
                    var p = document.createElement("p");
                    p.innerHTML = "Enter password for \"" + connectSsid + "\"";
                    dialogDiv.appendChild(p)
                    var input = document.createElement("input");
                    input.type = "password";
                    input.placeholder = "Please enter a password..."
                    input.minLength = 8;
                    input.setAttribute("required", "");
                    input.style.padding = "8px 12px";
                    input.style.border = "1px solid transparent";
                    dialogDiv.appendChild(input);
                    var button = document.createElement("button");
                    button.innerHTML = "Connect";
                    button.className = "footer-button";
                    button.style.float = "none";
                    dialogDiv.appendChild(button);
                    button.onclick = function() {
                        if (!input.checkValidity()) {
                            alert(input.validationMessage);
                            return;
                        }
                        self.clientDatabase[elementIndex].psk = btoa(input.value);
                        applyConf();
                        self.connectNetwork(connectSsid);
                    }
                    openDialog(dialogDiv);
                }
            }
        }
        tr.appendChild(td);
        td = document.createElement("td");
        td.style.padding = "0px 16px 0px 16px";
        var span = document.createElement('span');
        span.className = "delete";
        span.onclick = function() {
            self.removeElement(newClientNetwork.ssid);
            self.changed = true;
            dataChanged();
        }
        span.appendChild(document.createTextNode("\u00D7"));
        td.appendChild(span);
        var img = document.createElement("img");
        td.appendChild(img);
        tr.appendChild(td);
        table.appendChild(tr);
    }

    removeElement(removeSsid) {
        var elementIndex = this.clientDatabase.findIndex(x => x.ssid == removeSsid);
        if(elementIndex >= 0) {
            this.clientDatabase.splice(elementIndex,1);
            console.info("Remove client network \"" + removeSsid + "\" from database");
            var table = document.getElementById("wlan-client-network-db").getElementsByTagName("tbody")[0];
            table.removeChild(table.rows[elementIndex+1]);
        }
    }
    
    wlanClientScanListClear() {
        var table = document.getElementById("wlan-client-network-scan");
        var rowCount = table.rows.length;
        if(rowCount == 1) return;
        for(var i = 1; i < rowCount; i++) {
            table.deleteRow(1);
        }
        table.style = "tr:nth-child(even), tr:nth-child(even) input { background-color: #f2f2f2; }"
    }

    wlanClientScanListAddElement(newClientNetwork) {
        var elementIndex = this.clientDatabase.findIndex(x => x.ssid == newClientNetwork.ssid);
        if(elementIndex < 0) {
            var self = this;
            var table = document.getElementById("wlan-client-network-scan").getElementsByTagName("tbody")[0];
            var tr = document.createElement('tr');
            var td = document.createElement('td');
            td.appendChild(document.createTextNode(newClientNetwork.ssid));
            tr.appendChild(td);
            td = document.createElement('td');
            td.style.padding = "0px 16px 0px 16px";
            var span = document.createElement('span');
            span.className = "add";
            span.onclick = function() {
                if(self.clientDatabase.length >= self.clientDatabaseLimit) {
                    alert("Database limit of " + self.clientDatabaseLimit + " networks reached");
                    return;
                }
                var row = this.parentElement.parentElement;
                var index = row.rowIndex - 1;
                var td = row.getElementsByTagName("td");
                var addSsid = td[0].innerHTML;

                var dialogDiv = document.createElement("div");
                var p = document.createElement("p");
                p.innerHTML = "Enter password for \"" + addSsid + "\"";
                dialogDiv.appendChild(p)
                var input = document.createElement("input");
                input.type = "password";
                input.placeholder = "Please enter a password..."
                input.minLength = 8;
                input.setAttribute("required", "");
                dialogDiv.appendChild(input);
                var button = document.createElement("button");
                button.innerHTML = "Add";
                dialogDiv.appendChild(button);
                button.onclick = function() {
                    if (!input.checkValidity()) {
                        alert(input.validationMessage);
                        return;
                    }
                    var ret = self.addCallback(addSsid, btoa(input.value));
                    if(ret) {
                        table.removeChild(row);
                        table.style = "tr:nth-child(even), tr:nth-child(even) input { background-color: #f2f2f2; }"
                        applyConf();
                        closeDialog();
                    }
                    else {
                        closeDialog();
                    }
                }
                openDialog(dialogDiv);
            }
            span.appendChild(document.createTextNode("+"));
            td.appendChild(span);
            var img = this.generateRssiImage(newClientNetwork.rssi);
            td.appendChild(img);
            tr.appendChild(td);
            table.appendChild(tr);
        }
        else {
            console.info("Network \"" + newClientNetwork.ssid + "\" is already in database");
            var table = document.getElementById("wlan-client-network-db").getElementsByTagName("tbody")[0];
            var img = this.generateRssiImage(newClientNetwork.rssi);
            var tr = table.rows[elementIndex+1];
            var oldImg = tr.cells[1].getElementsByTagName("img");
            tr.cells[1].replaceChild(img, oldImg[0]);
        }
    }

    generateRssiImage(rssi) {
        var img = document.createElement("img");
        if (rssi < 0 && rssi > -70)
            img.src = "img/rssiTop.png";
        else if (rssi < -71 && rssi > -85)
            img.src = "img/rssiGood.png";
        else if (rssi < -86 && rssi > -100)
            img.src = "img/rssiFair.png";
        else if (rssi < -100 && rssi > -110)
            img.src = "img/rssiBad.png";
        else
            img.src = "img/rssiNone.png";
        img.height = 40;
        img.width = 40;
        img.style.float = "right";
        img.style.paddingRight = "16px";

        return img
    }

    updateScanTable(scanResults) {
        this.wlanClientScanListClear();
        for(var i = 0; i < scanResults.length; i++) {
            this.wlanClientScanListAddElement(scanResults[i]);
        }
    }

    wlanClientScan(self) {
        ajaxJson("GET", "wifiScan", function (c) {
            if (c.length > 0) {
                self.updateScanTable(c);
            }
            else {
                console.error(c.error);
                alert("Scan error occured, please rescan" + c.error);
            }
        }, function (b, a) {
            alert("Scan error occured, please rescan");
        })
    }
}