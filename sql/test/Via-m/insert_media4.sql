select add_media((select event_id from event where event_name = 'event 1'), (select max(media_description_id) from media_description), 'identifier3', 1, 25, 125);
