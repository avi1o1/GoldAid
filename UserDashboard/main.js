document.addEventListener("DOMContentLoaded", function () {
  const CONFIG = {
    vitalsChannelID: "2787553",
    vitalsReadAPI: "B9VN706EGIISH9L5",
    alertsChannelID: "2787587",
    alertsReadAPI: "1FM4WTY1P5LEXSIV",
    updateInterval: 2000,
  };

  function updateTable(nodeData) {
    const tableBody = document.querySelector("#dataTable tbody");
    tableBody.innerHTML = "";
    
    if (nodeData.feeds && nodeData.feeds.length > 0) {
      const latestEntry = nodeData.feeds[0];
      
      // loop to print all values from field1 to field8
      for (let i = 1; i <= 8; i++) {
        data = latestEntry[`field${i}`] || "NULL";
        if (data === "NULL") {
          continue;
        }
        
        // Extract details in format "<bsd_id>-<wsd_id>:<temp>,<spo2>,<hr>,<lat>,<lon>:...;"
        const [bsd_id, data] = data.split("-");
        const dataPoints = data.split(";");

        // iterate over each data point and extract values
        for (let i = dataPoints.length-1; i >= 0; i--) {
          const dataPoint = dataPoints[i];

          const [wsd_id, vitals] = dataPoint.split(":");
          const [temp, spo2, hr, lat, lon] = vitals.split(",");
          const cellValues = [wsd_id, bsd_id, temp, spo2, hr, `${lat}, ${lon}`, latestEntry.updated_at];
          // console.log(cellValues);

          // Add values to table
          const valuesRow = document.createElement("tr");
          const nodeCell = document.createElement("td");
          nodeCell.textContent = `${nodeData.channel.id}`;
          valuesRow.appendChild(nodeCell);
  
          cellValues.forEach((value, index) => {
            const cell = document.createElement("td");
            cell.textContent = value;
            valuesRow.appendChild(cell);
          });
          
          tableBody.appendChild(valuesRow);
        }
      }
    }

    if (tableBody.children.length === 0) {
      console.log("No data or empty feeds for all nodes.");
      const errorRow = document.createElement("tr");
      const errorCell = document.createElement("td");
      errorCell.setAttribute("colspan", 8);
      errorCell.textContent = "No data or empty feeds for all nodes.";
      errorCell.classList.add("error-message");
      errorRow.appendChild(errorCell);
      tableBody.appendChild(errorRow);  
    }
  }

  function updateFall(nodeData) {
    if (nodeData.feeds && nodeData.feeds.length > 0) {
      const latestEntry = nodeData.feeds[0];
      
      // loop to print all values from field1 to field8
      for (let i = 1; i <= 8; i++) {
        data = latestEntry[`field${i}`] || "NULL";
        if (data === "NULL") {
          continue;
        }
        
        // Extract details in format "bsd_id-wsd_id;..."
        const dataPoints = data.split(";");
        const tableBody = document.querySelector("#dataTable tbody");
        const rows = tableBody.children;
        
        dataPoints.forEach((dataPoint) => {
          const [ids, vitals] = dataPoint.split(":");
          const [bsd_id, wsd_id] = ids.split("-");
        
          for (let i = 0; i < rows.length; i++) {
            const row = rows[i];
            if (row.children[1].textContent === wsd_id) {
              row.classList.add("fall-detected");
              break;
            }
          }
        });
      }
    }
  }

  function updateData() {
    // Fetching Vital data from ThingSpeak
    fetch(
      `https://api.thingspeak.com/channels/${CONFIG.vitalsChannelID}/feeds.json?api_key=${CONFIG.vitalsReadAPI}&results=1`
    )
      .then((response) => {
        if (!response.ok) {
          throw new Error("Network response was not ok");
        }
        return response.json();
      })
      .then(updateTable)
      .catch((error) =>
        console.error("Error fetching or updating data:", error.message)
      );

    // Fetching Alert data from ThingSpeak
    fetch(
      `https://api.thingspeak.com/channels/${CONFIG.alertsChannelID}/feeds.json?api_key=${CONFIG.alertsReadAPI}&results=1`
    )
      .then((response) => {
        if (!response.ok) {
          throw new Error("Network response was not ok");
        }
        return response.json();
      })
      .then(updateFall)
      .catch((error) =>
        console.error("Error fetching or updating data:", error.message)
      );
  }

  // Initial data fetch and interval update
  updateData();
  setInterval(updateData, CONFIG.updateInterval);
});
