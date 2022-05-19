function checkHVML() {
    console.log(typeof(document.getElementByHVMLHandle));
    console.log(typeof(HVML));
    console.log(HVML.version);

    if (typeof(document.getElementByHVMLHandle) == "function") {
        if (typeof(HVML) == "object" && HVML.version >= 10) {
            var elemStatus = document.getElementByHVMLHandle('731128');
            var elemRunner = document.getElementByHVMLHandle('790715');
            if (elemStatus && elemRunner) {
                elemStatus.textContent = 'Ready';
                elemRunner.textContent = '@' + HVML.hostName + '/' + HVML.appName + '/' + HVML.runnerName;
                return true;
            }
            else {
                console.log("Make sure to use the correct version of xGUI Pro");
            }
        }
        else {
            console.log("Make sure to use the correct version of xGUI Pro");
        }
    }
    else {
        console.log("Make sure to use the tailored WebKit by HVML community and the configuration option `ENABLE_HVML_ATTRS` is ON.");
    }

    return false;
}

if (checkHVML()) {
    HVML.onmessage = function (msg) {
        console.log("HVML.onmessage called");
    }
}

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
    log.console();
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
