document.addEventListener("DOMContentLoaded", function () {
  const CONFIG = {
    vitalsChannelID: "2787553",
    vitalsReadAPI: "B9VN706EGIISH9L5",
    alertsChannelID: "2787587",
    alertsReadAPI: "1FM4WTY1P5LEXSIV",
    updateInterval: 2000,
  };

  // Helper function to manage cookies
  function setCookie(name, value, days) {
    const date = new Date();
    date.setTime(date.getTime() + (days * 24 * 60 * 60 * 1000));
    const expires = "expires=" + date.toUTCString();
    document.cookie = name + "=" + value + ";" + expires + ";path=/";
  }

  function getCookie(name) {
    const cookieName = name + "=";
    const cookies = document.cookie.split(';');
    for (let i = 0; i < cookies.length; i++) {
      let cookie = cookies[i].trim();
      if (cookie.indexOf(cookieName) === 0) {
        return cookie.substring(cookieName.length, cookie.length);
      }
    }
    return "";
  }

  function updateTable(nodeData) {
    const tableBody = document.querySelector("#dataTable tbody");

    if (nodeData.feeds && nodeData.feeds.length > 0) {
      const latestEntry = nodeData.feeds[0];

      for (let i = 1; i <= 8; i++) {
        let content = latestEntry[`field${i}`] || "NULL";
        if (content === "NULL") continue;

        const [bsd_id, data] = content.split("-");
        const dataPoints = data.split(";");

        dataPoints.forEach(dataPoint => {
          if (dataPoint === "") return;

          const [wsd_id, vitals] = dataPoint.split(":");
          if (!wsd_id || !vitals) return;

          const [temp, spo2, hr, lat, lon] = vitals.split(",");
          const cellValues = [
            wsd_id,
            bsd_id,
            temp,
            spo2,
            hr,
            `${lat}, ${lon}`,
            new Date(latestEntry.created_at).toLocaleString()
          ];

          // Check for existing row with matching wsd_id
          const existingRow = Array.from(tableBody.children)
            .find(row => row.children[0]?.textContent === wsd_id);

          if (existingRow) {
            // Update existing row
            cellValues.forEach((value, index) => {
              existingRow.children[index].textContent = value;
            });
          } else {
            // Add new row
            const newRow = document.createElement("tr");
            cellValues.forEach(value => {
              const cell = document.createElement("td");
              cell.textContent = value;
              newRow.appendChild(cell);
            });

            // Add Resolve button cell
            const buttonCell = document.createElement("td");
            const resolveButton = document.createElement("button");
            resolveButton.textContent = "Resolve";
            resolveButton.classList.add("resolve-button");
            buttonCell.style.display = "none";

            // Add click handler for resolve button
            resolveButton.addEventListener("click", function () {
              const timestamp = new Date(latestEntry.created_at).getTime();
              const buttonCell = resolveButton?.parentElement;
              newRow.classList.remove("fall-detected");
              buttonCell.style.display = "none";
              resolveButton.style.display = "none";
              
              setCookie(`resolved_${wsd_id}`, timestamp, 30);
            });

            buttonCell.appendChild(resolveButton);
            newRow.appendChild(buttonCell);

            tableBody.appendChild(newRow);
          }
        });
      }
    }

    // Show error if table is empty
    if (tableBody.children.length === 0) {
      const errorRow = document.createElement("tr");
      const errorCell = document.createElement("td");
      errorCell.setAttribute("colspan", 8);
      errorCell.textContent = "No data or empty feeds for all nodes.";
      errorCell.classList.add("error-message");
      errorRow.appendChild(errorCell);
      tableBody.appendChild(errorRow);
    } else {
      const errorRow = tableBody.querySelector('.error-message')?.parentElement;
      if (errorRow) {
        errorRow.remove();
      }
    }
  }

  function updateFall(nodeData) {
    if (nodeData.feeds && nodeData.feeds.length > 0) {
      const latestEntry = nodeData.feeds[0];
      const currentTimestamp = new Date(latestEntry.created_at).getTime();

      // loop to print all values from field1 to field8
      for (let i = 1; i <= 8; i++) {
        let data = latestEntry[`field${i}`] || "NULL";
        if (data === "NULL") {
          continue;
        }

        // Extract details in format "<bsd_id>-<wsd_id>;..."
        const [bsd_id, wsd_ids_str] = data.split("-");
        const wsd_ids = wsd_ids_str.split(";");
        const tableBody = document.querySelector("#dataTable tbody");
        const rows = tableBody.children;

        wsd_ids.forEach((wsd_id) => {
          if (wsd_id === "") {
            return;
          }

          // Check if this alert is after the last resolved time
          const lastResolved = getCookie(`resolved_${wsd_id}`);
          const shouldShowAlert = !lastResolved || currentTimestamp > parseInt(lastResolved);

          for (let i = 0; i < rows.length; i++) {
            const row = rows[i];
            if (row.children[0].textContent === wsd_id) {
              if (shouldShowAlert) {
                row.classList.add("fall-detected");
                // Show resolve button
                const resolveButton = row.querySelector(".resolve-button");
                const buttonCell = resolveButton?.parentElement;
                if (resolveButton && buttonCell) {
                    buttonCell.style.display = "table-cell";
                    resolveButton.style.display = "block";
                }
              }
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