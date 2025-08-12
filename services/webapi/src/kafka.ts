import { Kafka, logLevel } from 'kafkajs';
import { cfg } from './config.js';
import { broadcast } from './sse.js';

export async function startKafkaSSE() {
  const kafka = new Kafka({
    brokers: cfg.kafka.brokers,
    clientId: 'finnews-webapi',
    logLevel: logLevel.NOTHING
  });

  const consumer = kafka.consumer({ groupId: cfg.kafka.groupSse });
  await consumer.connect();
  await consumer.subscribe({ topic: cfg.kafka.scoredTopic, fromBeginning: false });

  await consumer.run({
    eachMessage: async ({ message }) => {
      if (!message.value) return;
      try {
        const row = JSON.parse(message.value.toString('utf8'));
        // broadcast every scored row
        broadcast('scored', row);
      } catch {}
    }
  });

  return consumer;
}
