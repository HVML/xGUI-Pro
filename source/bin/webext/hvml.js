window.onload = function() {
    alert("Loaded - hvml.js");
    var elem = document.getElementById('endpoint');
    elem.textContent = '@' + HVML.hostName + '/' + HVML.appName + '/' + HVML.runnerName;
};

/*
function onChangePage(msg)
{
    if (msg.operation === 'load') {
        document.open();
        document.write(msg.data);
        document.close();
    }
    else if (msg.operation === 'writeBegin') {
        document.open();
        document.write(msg.data);
    }
    else if (msg.operation === 'writeMore') {
        document.write(msg.data);
    }
    else if (msg.operation === 'writeEnd') {
        document.write(msg.data);
        document.close();
    }

    // check all elements which defined hvml:events and listen the events
}

function onChangeDocument(msg)
{
    var elements;

    if (msg.elementType === 'css') {
        elements = document.querySelectorAll(msg.elementValue);
    }
    else if (msg.elementType === 'handle') {
        elements = document.getElementByHVMLHandle(msg.elementValue);
    }

    if (msg.operation === 'append') {
    }
    else if (msg.operation === 'prepend') {
    }
}

HVML.onmessage = function (msg) {
    if (msg.type === 'request' && msg.target !== 'dom') {
        return onChangePage(msg);
    }

    if (msg.type === 'request' && msg.target === 'dom') {
        if (msg.operation != 'request') {
            return onChangeDocument(msg);
        }
        else if (msg.operation.startsWith('callMethod') {
            return onCallMethod(msg);
        }
    }

}
*/
