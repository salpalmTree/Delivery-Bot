var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onload); 
var numRoutes;
var thestate; 

function onload(event)
{
    initWebSocket();
}

function initWebSocket() 
{
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen; 
    websocket.onclose = onClose; 
    websocket.onmessage = onMessage;
}

function onOpen(event) 
{
    console.log('Connection Opened'); 
}

function onClose(event)
{
    console.log('Connection Closed'); 
    setTimeout(initWebSocket, 2000);
}

function submitForm() 
{
    const rbs = document.querySelectorAll('input[name="numRoutes"]'); 
    var numRoutes;
    for (const rb of rbs)
    {
        if(rb.checked)
        {
            numRoutes = rb.value;
            break;
        }
    }
    document.getElementById("state").innerHTML = "processing order...";
    document.getElementById("state").style.color = "blue";
    if (numRoutes=="1")
    {
        document.getElementById("gear").classList.add("spin");
    }
    else if (numRoutes=="2")
    {
        document.getElementById("gear").classList.add("spin-back");
    }
    else
    {
        document.getElementById("gear").classList.add("spin"); 
    }

    var routes = document.getElementById("route").value;
    websocket.send(numRoutes+"$"+routes);
}

function onMessage(event)
{
    console.log(event.data);
    thestate = event.data;
    if(thestate=="notcalib")
    {
        document.getElementById("state").innerHTML = "Uncalibrated"; 
        document.getElementById("state").style.color = "red"; 
        document.getElementById("gear").classList.remove("spin", "spin-back"); 
    }
    else if(thestate=="calib")
    {
        document.getElementById("state").innerHTML = "Calibrated"; 
        document.getElementById("state").style.color = "green"; 
        document.getElementById("gear").classList.remove("spin", "spin-back");
    }
    else if(thestate=="delivery")
    {
        document.getElementById("state").innerHTML = "On the way..."; 
        document.getElementById("state").style.color = "blue"; 
        document.getElementById("gear").classList.remove("spin", "spin-back");
        document.getElementById("gear").classList.add("spin");
    }
    else if(thestate=="path")
    {
        document.getElementById("state").innerHTML = "Where to boss?"; 
        document.getElementById("state").style.color = "blue"; 
        document.getElementById("gear").classList.remove("spin", "spin-back"); 
    }
    else if(thestate =="drop")
    {
        document.getElementById("state").innerHTML = "Delivering..."; 
        document.getElementById("state").style.color = "green";
        document.getElementById("gear").classList.remove("spin", "spin-back");
        document.getElementById("gear").classList.add("spin-back"); 
    }
    else if(thestate=="pick")
    {
        document.getElementById("state").innerHTML = "Picking Up..."; 
        document.getElementById("state").style.color = "blue"; 
        document.getElementById("gear").classList.remove("spin", "spin-back");
        document.getElementById("gear").classList.add("spin-back"); 
    }
}