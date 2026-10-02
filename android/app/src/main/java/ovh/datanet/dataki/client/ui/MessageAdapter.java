package ovh.datanet.dataki.client.ui;

import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.List;

import ovh.datanet.dataki.client.R;

/** RecyclerView adapter rendering user/assistant bubbles. */
public class MessageAdapter extends RecyclerView.Adapter<MessageAdapter.VH> {

    private final List<Message> items = new ArrayList<>();

    public int add(Message m) {
        items.add(m);
        int pos = items.size() - 1;
        notifyItemInserted(pos);
        return pos;
    }

    public Message get(int pos) {
        return items.get(pos);
    }

    public void changed(int pos) {
        notifyItemChanged(pos);
    }

    public void clear() {
        int n = items.size();
        items.clear();
        notifyItemRangeRemoved(0, n);
    }

    @Override
    public int getItemCount() {
        return items.size();
    }

    @NonNull
    @Override
    public VH onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View v = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.item_message, parent, false);
        return new VH(v);
    }

    @Override
    public void onBindViewHolder(@NonNull VH holder, int position) {
        Message m = items.get(position);
        boolean user = Message.ROLE_USER.equals(m.role);
        holder.text.setText(m.content.toString());
        holder.text.setBackgroundResource(user ? R.drawable.bubble_user : R.drawable.bubble_assistant);

        FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams) holder.text.getLayoutParams();
        lp.gravity = user ? Gravity.END : Gravity.START;
        holder.text.setLayoutParams(lp);
    }

    static final class VH extends RecyclerView.ViewHolder {
        final TextView text;
        VH(View v) {
            super(v);
            text = v.findViewById(R.id.messageText);
        }
    }
}
