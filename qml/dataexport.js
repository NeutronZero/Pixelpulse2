var csvEscape = function(value) {
    var text = '' + value;
    if (/[",\r\n]/.test(text)) {
        return '"' + text.replace(/"/g, '""') + '"';
    }
    return text;
};

var dumpSamples = function(columns, labels) {
    if (columns.length !== labels.length) {
        throw new Error("label length mismatches number of columns");
    }
    if (columns.length === 0) {
        return "";
    }

    var lengths = columns.map(function(column) { return column.length; });
    var minimumLength = Math.min.apply(null, lengths);
    var csvContent = labels.map(csvEscape).join(",") + "\n";
    for (var i = 0; i < minimumLength; i++) {
        var row = [];
        for (var j = 0; j < columns.length; j++) {
            var value = Number(columns[j][i]);
            row.push(isFinite(value) ? value.toFixed(4) : "");
        }
        csvContent += row.join(",") + "\n";
    }
    return csvContent;
};

var saveData = function(target) {
    var labels = [];
    var columns = [];
    if (!session.devices.length) {
        return false;
    }

    for (var i = 0; i < session.devices.length; i++) {
        var device = session.devices[i];
        for (var j = 0; j < device.channels.length; j++) {
            var channel = device.channels[j];
            for (var k = 0; k < channel.signals.length; k++) {
                var signal = channel.signals[k];
                labels.push(i + channel.label + "_" + signal.label);
                columns.push(signal.buffer.getData());
            }
        }
    }

    return fileio.writeByURI(target, dumpSamples(columns, labels));
};
