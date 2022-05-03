var intervallId;

function startSensorValues() {
    intervallId = setInterval(getSensorValues, 1000);
}

function getSensorValues() {
    ajaxJson("GET", "sensor/getValues", function (c) {
        if (Object.keys(c.result).length > 0) {
            document.getElementById("voltage-supply").innerHTML = c.result.vcc;
            document.getElementById("voltage-supply").innerHTML += " V";
            document.getElementById("voltage-bat").innerHTML = c.result.vbat;
            document.getElementById("voltage-bat").innerHTML += " V";
            document.getElementById("voltage-solar").innerHTML = c.result.vsolar;
            document.getElementById("voltage-solar").innerHTML += " V";
            document.getElementById("temperature-soil").innerHTML = c.result.tempsoil;
            document.getElementById("temperature-soil").innerHTML += " °C";
            document.getElementById("moisture-soil").innerHTML = c.result.humsoil;
            document.getElementById("moisture-soil").innerHTML += " %";
        }
    }, function (b, a) {
        console.debug("getValues failed !");
    })
}

function stopSensorValues() {
    clearInterval(intervallId);
}

function calibrateSensor(id) {
    if(id == undefined) return;
    var url = "calibSensor?limit=" + id;
    ajaxJson("GET", url, function (c) {
      if(c.result = false) {
        console.error(c.result.error);
        alert("Could not calibrate sensor!\n" + c.result.error);
      }
    }, function (b, a) {
        alert("Could not calibrate sensor!");
    })
  }