function meter(elem, config){

	//User Config
	var yellowVal = config.yellowVal || 750;
	var redVal = config.redVal || 1000;
	var textVal = config.textVal || "PWR fwd";
	var textFont = config.textFont || "8px Arial";
	var textOffset = config.textOffset || 70;
	var peakSpeed = config.peakSpeed || 2;
	var peakHold = config.peakHold || 20;
	var peakWidth = config.peakWidth || 10;
	var maxVal = config.maxVal || 1250;
	var minVal = config.minVal || 0;
	var unit = config.unit || "";
	var decimals = config.decimals || 0;

    // Derived and starting values
    var width = elem.width - 10;
    var height = elem.height ;
	var peakHoldTimer = 0;
	var peakVal = 0;
	var textColor = 'rgba(0,0,0)';
    var red = 'rgba(255,47,30)';
    var yellow  = 'rgba(255,215,5)';
    var green = 'rgba(53,255,30)';

    // Canvas starting state
    var c = elem.getContext('2d');
	
    // Main draw loop
    var draw = function(){

        var barVal = parseFloat(elem.dataset.val);
		
		//if (barVal > maxVal) {barVal = maxVal;}
		if (barVal < minVal) {barVal = minVal;}
		
		
		//Peak Position ausrechnen
		if (barVal >= peakVal) { 
			peakVal = barVal; 
			peakHoldTimer = peakHold;
		} else {
			if (peakHoldTimer > 0) {
				peakHoldTimer = peakHoldTimer - 1;
			} else {
				if (peakVal > 0) {peakVal = peakVal - peakSpeed / ((width - textOffset) / maxVal) };
			}
		}
		
		//Peak anzeigen
		if (peakVal < 0) {peakVal = 0;}
		try {document.getElementById("label" + elem.id).innerHTML = Math.round(peakVal * decimals) / decimals  + unit; } catch {}

        c.save(); 

		//Löschen
		c.beginPath();
		c.clearRect(0, 0, width + 100, height);

		//Bar Positionen berechnen
		var barGreenStart = textOffset
		var barGreenLength = (width - textOffset) / maxVal * barVal
		var barYellowStart = textOffset + (width - textOffset) / maxVal * yellowVal
		var barYellowLength = 0;
		var barRedStart = textOffset + (width - textOffset) / maxVal * redVal
		var barRedLength = 0;
		//Übergang von grün auf gelb
		if ((barGreenLength + barGreenStart) >= barYellowStart) {
			barYellowLength = barGreenStart + barGreenLength - barYellowStart;
			barGreenLength = barYellowStart - barGreenStart;
		}
		//Übergang von gelb auf rot
		if ((barYellowLength + barYellowStart) >= barRedStart) {
			barRedLength = barYellowStart + barYellowLength - barRedStart;
			barYellowLength = barRedStart - barYellowStart;
		}
		//Green Bar
		c.beginPath();
		c.lineWidth = 0;
		c.fillStyle = green;
		c.fillRect(barGreenStart, 0, barGreenLength, height - 10);
		//Yellow Bar
		c.beginPath();
		c.lineWidth = 0;
		c.fillStyle = yellow;
		c.fillRect(barYellowStart, 0, barYellowLength, height - 10);
		//Red Bar
		c.beginPath();
		c.lineWidth = 0;
		c.fillStyle = red;
		c.fillRect(barRedStart, 0, barRedLength, height - 10);

		//Peak malen
		var peakPosition = textOffset - peakWidth + (width - textOffset) / maxVal * peakVal 
		if (peakPosition > barGreenStart) {
			c.beginPath();
			if (peakPosition > barGreenStart ) {c.fillStyle = green;} 
			if (peakPosition > barYellowStart) {c.fillStyle = yellow;}
			if (peakPosition > barRedStart) {c.fillStyle = red;}
			c.fillRect(peakPosition, 0, peakWidth, height - 10);
		}

		//Text
		c.beginPath();
		c.font = "13px Arial";
		c.textBaseline = "middle";
		c.textAlign = "right";
		c.fillStyle = textColor;
		c.fillText(textVal, textOffset - 5, height / 2);

		//Anfangslinie malen
		c.beginPath();
		c.moveTo(textOffset, 0);
		c.lineTo(textOffset, height - 10);
		c.lineWidth = 0.75;
		c.strokeStyle = textColor;
		c.stroke();

		//untere Linie malen
		c.beginPath();
		c.moveTo(textOffset, height - 10);
		c.lineTo(width, height - 10);
		c.lineWidth = 0.75;
		c.strokeStyle = textColor;
		c.stroke();

		//kleine Linien malen
		smallTickCount = 100;
		for (i = 1; i <= smallTickCount; i++) {
			x = textOffset + (width - textOffset) / smallTickCount * i;
			c.beginPath();
			c.moveTo(x, height - 15);
			c.lineTo(x, height - 10);
			c.lineWidth = 1;
			c.strokeStyle = textColor;
			c.stroke();
		}

		//große Linien malen
		bigTickCount = 10;
		for (i = 0; i <= bigTickCount; i++) {
			x = textOffset + (width - textOffset) / bigTickCount * i;
			if (i > 0) {
				c.beginPath();
				c.moveTo(x, height - 18);
				c.lineTo(x, height - 10);
				c.lineWidth = 2;
				c.strokeStyle = textColor;
				c.stroke();
			}
			//Beschriftung
			v = maxVal / bigTickCount * i;
			c.font = textFont;
			c.textBaseline = "bottom";
			c.textAlign = "center";
			c.fillText(v, x, height);
		}
				
        c.restore();
		setTimeout(draw, 40);
    };

	// Trigger the animation
	draw();
	
}

